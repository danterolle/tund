# Tund — Troubleshooting

## Network basics

- Tund listens on UDP port `9909` by default. Allow inbound UDP on the server firewall; clients normally need only outbound UDP.
- The virtual subnet is fixed at `10.9.0.0/24`. Do not use Tund on a host already routing that subnet through a real LAN or another VPN.
- All participants must run the same Tund protocol version: authenticated framing is not compatible with older builds.

## Connectivity checks

After a client connects and receives a virtual IP, try:

```bash
ping 10.9.0.1
```

If ping fails but registration succeeded, ICMP may be blocked by a firewall. Test TCP traffic with `nc` as described in [Usage](USAGE.md#verify-it-works).

## Windows notes

- **Administrator privileges**: Tund creates a TUN interface, so Windows may show a UAC prompt on startup. Accept it to continue.
- **Wintun**: keep `wintun.dll` next to `tund.exe`. The release zip already bundles both files.
- **Server firewall**: Tund does not change Windows Firewall automatically. If this machine is the server, allow inbound UDP on port `9909` or clients will not be able to reach it.

```cmd
:: Persistent rule (recommended)
netsh advfirewall firewall add rule name="Tund" dir=in action=allow protocol=udp localport=9909

:: Temporarily disable (test only)
netsh advfirewall set allprofiles state off
netsh advfirewall set allprofiles state on
```

- **ICMP (ping)**: Windows Firewall often blocks ICMP echo requests on the virtual adapter. Ping is not required for game traffic, only for connectivity checks.
- **Console encoding**: `tund.exe` sets UTF-8 automatically. If characters display incorrectly in older terminals, run `chcp 65001` before launching the program.

## macOS notes

- **Unverified developer warning**: the first time you run Tund, macOS may show "Apple cannot verify the identity of the developer". This happens because the binary is not signed with an Apple Developer certificate. To bypass:

```bash
xattr -d com.apple.quarantine ./tund
```

Run this once before launching. If the warning persists, open **System Settings → Privacy & Security** and click **Open Anyway** next to the blocked Tund entry.

- **TUN interface**: macOS creates a `utun` device automatically when Tund starts. You may see a prompt asking for network configuration permission; enter your password to allow it.
- **ICMP (ping)**: like Windows, macOS may block ICMP on the virtual interface. Ping is not required for game traffic.

## Common symptoms

| Symptom | Likely cause |
|---|---|
| Client times out during registration | Wrong server address, blocked UDP port, server not running, or firewall issue |
| Authentication failure | Shared key mismatch or incompatible protocol version |
| Route/subnet error | Another VPN or LAN already uses `10.9.0.0/24` |
| Ping fails but peer is connected | ICMP blocked by host firewall |
| Windows cannot load Wintun | `wintun.dll` missing or not beside `tund.exe` |
| TUI flickers in old `cmd.exe` | Use Windows Terminal or run with `--no-tui` |
