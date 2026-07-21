# TunD — Troubleshooting

## Network basics

- TunD listens on UDP port `9909` by default. Allow inbound UDP on the server firewall; clients normally need only outbound UDP.
- The virtual subnet is fixed at `10.9.0.0/24`. Do not use TunD on a host already routing that subnet through a real LAN or another VPN.
- All participants must run the same TunD protocol version: encrypted framing is not compatible with older builds.
- If every machine can already reach the game host on the same physical LAN, TunD may be unnecessary.

## Connectivity checks

After a client connects and receives a virtual IP, try:

```bash
ping 10.9.0.1
```

If ping fails but registration succeeded, ICMP may be blocked by a firewall. Test TCP traffic with `nc` as described in [Usage](USAGE.md#verify-it-works).

## Windows notes

- **Administrator privileges**: TunD creates a TUN interface, so Windows may show a UAC prompt on startup. Accept it to continue. The GUI relaunches through UAC before it opens. If you use the standalone CLI with `--key-stdin`, run it from an Administrator terminal because stdin cannot be carried through an automatic UAC relaunch.
- **Wintun**: keep `wintun.dll` next to `tund-cli.exe`. The release zip already bundles both files.
- **Server firewall**: TunD does not change Windows Firewall automatically. If this machine is the server, allow inbound UDP on port `9909` or clients will not be able to reach it.

```cmd
:: Persistent rule (recommended)
netsh advfirewall firewall add rule name="TunD" dir=in action=allow protocol=udp localport=9909

:: Temporarily disable (test only)
netsh advfirewall set allprofiles state off
netsh advfirewall set allprofiles state on
```

- **ICMP (ping)**: Windows Firewall often blocks ICMP echo requests on the virtual adapter. Ping is not required for game traffic, only for connectivity checks.
- **Console encoding**: `tund-cli.exe` sets UTF-8 automatically. If characters display incorrectly in older terminals, run `chcp 65001` before launching the program.

## macOS notes

- **Unverified developer warning**: the first time you run TunD, macOS may show "Apple cannot verify the identity of the developer". This happens because the binary is not signed with an Apple Developer certificate. To bypass:

```bash
xattr -dr com.apple.quarantine ./tund-gui-darwin-universal
xattr -d com.apple.quarantine ./tund-cli-darwin-universal
```

Run the relevant command once before launching the CLI or GUI. If the warning persists, open **System Settings → Privacy & Security** and click **Open Anyway** next to the blocked TunD entry.

- **TUN interface**: macOS creates a `utun` device automatically when TunD starts. For the GUI release, run `tund-gui.command` next to `tund-gui.app`; running `sudo tund-gui.app` targets the bundle directory and will not start the app with the required privileges.
- **ICMP (ping)**: like Windows, macOS may block ICMP on the virtual interface. Ping is not required for game traffic.

## Linux notes

- **Privileges**: TunD needs root or `CAP_NET_ADMIN` to create and configure the TUN device.
- **TUN device**: if `/dev/net/tun` is missing, load the kernel module:

```bash
sudo modprobe tun
```

- **Server firewall**: if this Linux host is the server, allow inbound UDP on the selected port:

```bash
sudo ufw allow 9909/udp
```

or, on firewalld-based systems:

```bash
sudo firewall-cmd --add-port=9909/udp --permanent
sudo firewall-cmd --reload
```

## Common symptoms

| Symptom | Likely cause |
|---|---|
| Client times out during registration | Wrong server address; blocked UDP port; server not running; firewall issue |
| Authentication failure | Shared key mismatch or incompatible protocol version |
| Route/subnet error | Another VPN or LAN already uses `10.9.0.0/24` |
| Ping fails but peer is connected | ICMP blocked by host firewall |
| Windows cannot load Wintun | `wintun.dll` missing or not beside `tund-cli.exe` |
| Linux cannot open `/dev/net/tun` | Missing TUN device; missing privileges; kernel module not loaded |
| TUI flickers in old `cmd.exe` | Use Windows Terminal or run with `--no-tui` |
