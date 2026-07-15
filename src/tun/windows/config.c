#ifdef _WIN32

#include "internal.h"

static uint8_t prefix_len_from_netmask(uint32_t netmask_nbo)
{
    uint32_t mask = ntohl(netmask_nbo);
    uint8_t prefix = 0;
    while (mask & 0x80000000U) {
        prefix++;
        mask <<= 1;
    }
    return prefix;
}

static bool same_luid(const NET_LUID *a, const NET_LUID *b)
{
    return a->Value == b->Value;
}

static bool windows_ip_config_applied(const tun_device_t *dev, uint32_t ip, uint32_t netmask)
{
    if (!dev->has_luid) return false;
    PMIB_UNICASTIPADDRESS_TABLE table = NULL;
    NETIO_STATUS status = GetUnicastIpAddressTable(AF_INET, &table);
    if (status != NO_ERROR) {
        LOG_DEBUG("GetUnicastIpAddressTable failed: %lu", status);
        return false;
    }

    uint8_t prefix = prefix_len_from_netmask(netmask);
    bool found = false;
    for (ULONG i = 0; i < table->NumEntries; i++) {
        MIB_UNICASTIPADDRESS_ROW *row = &table->Table[i];
        found = same_luid(&row->InterfaceLuid, &dev->luid) &&
                row->Address.si_family == AF_INET &&
                row->Address.Ipv4.sin_addr.s_addr == ip &&
                row->OnLinkPrefixLength == prefix;
        if (found) break;
    }
    FreeMibTable(table);
    return found;
}

static bool windows_route_applied(const tun_device_t *dev, uint32_t network, uint32_t netmask)
{
    if (!dev->has_luid) return false;
    PMIB_IPFORWARD_TABLE2 table = NULL;
    NETIO_STATUS status = GetIpForwardTable2(AF_INET, &table);
    if (status != NO_ERROR) {
        LOG_DEBUG("GetIpForwardTable2 failed: %lu", status);
        return false;
    }

    uint8_t prefix = prefix_len_from_netmask(netmask);
    bool found = false;
    for (ULONG i = 0; i < table->NumEntries; i++) {
        MIB_IPFORWARD_ROW2 *row = &table->Table[i];
        found = same_luid(&row->InterfaceLuid, &dev->luid) &&
                row->DestinationPrefix.Prefix.si_family == AF_INET &&
                row->DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr == network &&
                row->DestinationPrefix.PrefixLength == prefix;
        if (found) break;
    }
    FreeMibTable(table);
    return found;
}

static bool windows_add_onlink_route(const tun_device_t *dev, uint32_t network, uint32_t netmask)
{
    if (!dev->has_luid) return false;
    MIB_IPFORWARD_ROW2 row;
    InitializeIpForwardEntry(&row);
    row.InterfaceLuid = dev->luid;
    ConvertInterfaceLuidToIndex(&dev->luid, &row.InterfaceIndex);
    row.DestinationPrefix.Prefix.si_family = AF_INET;
    row.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr = network;
    row.DestinationPrefix.PrefixLength = prefix_len_from_netmask(netmask);
    row.NextHop.si_family = AF_INET;
    row.NextHop.Ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
    row.Metric = 2;

    NETIO_STATUS status = CreateIpForwardEntry2(&row);
    if (status == NO_ERROR || status == ERROR_OBJECT_ALREADY_EXISTS) return true;
    LOG_DEBUG("CreateIpForwardEntry2 failed: %lu", status);
    return false;
}

static bool windows_mtu_applied(const tun_device_t *dev, int mtu)
{
    if (!dev->has_luid) return false;
    MIB_IPINTERFACE_ROW row;
    InitializeIpInterfaceEntry(&row);
    row.Family = AF_INET;
    row.InterfaceLuid = dev->luid;
    NETIO_STATUS status = GetIpInterfaceEntry(&row);
    if (status != NO_ERROR) {
        LOG_DEBUG("GetIpInterfaceEntry failed: %lu", status);
        return false;
    }
    return row.NlMtu == (ULONG)mtu;
}

static bool wait_for_ip(const tun_device_t *dev, uint32_t ip, uint32_t mask)
{
    for (int i = 0; i < 20; i++) {
        if (windows_ip_config_applied(dev, ip, mask)) return true;
        Sleep(100);
    }
    return false;
}

static bool wait_for_route(const tun_device_t *dev, uint32_t network, uint32_t mask)
{
    for (int i = 0; i < 20; i++) {
        if (windows_route_applied(dev, network, mask)) return true;
        Sleep(100);
    }
    return false;
}

static bool wait_for_mtu(const tun_device_t *dev, int mtu)
{
    for (int i = 0; i < 20; i++) {
        if (windows_mtu_applied(dev, mtu)) return true;
        Sleep(100);
    }
    return false;
}

int tun_set_ip(tun_device_t *dev, uint32_t ip, uint32_t netmask)
{
    char buf[2048], ip_s[32], mask_s[32], net_s[32];
    struct in_addr ip_a = {.s_addr = ip};
    struct in_addr mask_a = {.s_addr = netmask};
    inet_ntop(AF_INET, &ip_a, ip_s, sizeof(ip_s));
    inet_ntop(AF_INET, &mask_a, mask_s, sizeof(mask_s));

    snprintf(buf, sizeof(buf), "interface ip set address name=\"%s\" static %s %s",
             dev->ifname, ip_s, mask_s);
    if (run_cmd("netsh", buf) < 0 || !wait_for_ip(dev, ip, netmask)) {
        LOG_ERROR("Failed to configure the Wintun IP address on %s.", dev->ifname);
        return -1;
    }

    uint32_t net = ntohl(ip) & ntohl(netmask);
    struct in_addr net_a = {.s_addr = htonl(net)};
    inet_ntop(AF_INET, &net_a, net_s, sizeof(net_s));
    if (!windows_route_applied(dev, net_a.s_addr, netmask) &&
        !windows_add_onlink_route(dev, net_a.s_addr, netmask)) {
        LOG_ERROR("Failed to add the Windows route for %s/%u.", net_s, prefix_len_from_netmask(netmask));
        return -1;
    }
    if (!wait_for_route(dev, net_a.s_addr, netmask)) {
        LOG_ERROR("Windows did not report route %s/%u on %s.", net_s, prefix_len_from_netmask(netmask), dev->ifname);
        return -1;
    }

    LOG_INFO("Configured %s: %s/%s", dev->ifname, ip_s, mask_s);
    return 0;
}

int tun_set_mtu(tun_device_t *dev, int mtu)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "interface ipv4 set subinterface \"%s\" mtu=%d store=active",
             dev->ifname, mtu);
    if (run_cmd("netsh", buf) < 0 || !wait_for_mtu(dev, mtu)) {
        LOG_ERROR("Failed to set Wintun MTU %d on %s.", mtu, dev->ifname);
        return -1;
    }
    dev->mtu = mtu;
    LOG_INFO("MTU set to %d on %s", mtu, dev->ifname);
    return 0;
}

#endif /* _WIN32 */
