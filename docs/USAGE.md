# TunD — Usage

## Pre-built releases

Pre-built binaries are available on the [releases page](https://github.com/danterolle/tund/releases).

Release assets include desktop GUI bundles and standalone CLI binaries:

| Asset | Contents |
|---|---|
| `tund-windows-x86_64.zip` | Full Windows bundle. Includes GUI + CLI + Flutter runtime + `wintun.dll`. Keep all files together. |
| `tund-gui-darwin-universal.zip` | macOS GUI bundle for Apple Silicon and Intel. Run `tund-gui.command` from the extracted folder. |
| `tund-cli-darwin-universal` | Standalone macOS CLI binary. |
| `tund-gui-linux-x86_64.tar.gz` | Linux desktop GUI bundle with the CLI included. |
| `tund-cli-linux-x86_64` | Standalone Linux CLI binary. |

The CLI examples below use `./tund-cli`; release binary names may differ. GUI-specific notes are in the [GUI README](../gui/README.md).

## Choose a network key

Use the same long random key on every endpoint.

```bash
openssl rand -base64 24 > tund.key
chmod 600 tund.key
```

The examples below use `tund.key`. Prefer `--key-file` or `--key-stdin` so the key does not appear in process lists or shell history.

## Start the server

On the machine that should act as the hub:

```bash
sudo ./tund-cli server --key-file tund.key
```

On Windows:

```powershell
.\tund-cli.exe server --key-file .\tund.key
```

Options:

```text
-k, --key <key>      Shared network key (visible in process list)
--key-file <path>    Read shared network key from a file
--key-stdin          Read shared network key from the first stdin line
-p, --port <port>    UDP port (default: 9909)
-t, --no-tui         Disable terminal UI (live peer dashboard)
-v, --verbose        Debug logging
```

## Connect as a client

```bash
sudo ./tund-cli client -s <server_ip> --key-file tund.key
```

On Windows:

```powershell
.\tund-cli.exe client -s <server_ip> --key-file .\tund.key
```

Options:

```text
-s, --server <ip>    Server IP/hostname (required)
-p, --port <port>    Server port (default: 9909)
-n, --name <name>    Display name (default: hostname)
-k, --key <key>      Shared network key (visible in process list)
--key-file <path>    Read shared network key from a file
--key-stdin          Read shared network key from the first stdin line
-t, --no-tui         Disable terminal UI (live peer dashboard)
-v, --verbose        Debug logging
```

## Example

```bash
# Machine A (server, e.g. IP 203.0.113.10):
sudo ./tund-cli server --key-file tund.key

# Machine B (client, behind NAT):
sudo ./tund-cli client -s 203.0.113.10 -n "Gaming-PC" --key-file tund.key

# Machine C (client, behind NAT):
sudo ./tund-cli client -s 203.0.113.10 -n "Work-Laptop" --key-file tund.key

# Now Machine B can ping Machine C:
ping 10.9.0.3
```

## Build

### Linux / macOS

```bash
make
```

### Windows

```bash
make windows
```

Run `tund-cli.exe` from the generated `dist` directory.

### Full local verification

```bash
make verify
```

This runs the full local verification set:

- C formatting
- C linting
- unit tests
- Peerforge UDP integration check
- Peerforge wrong-key rejection check
- sanitizer tests
- native build
- Windows cross-build

### Peerforge diagnostic tool

Build Peerforge, the UDP client simulator:

```bash
make tools
```

Run the local UDP integration check without creating real TUN interfaces:

```bash
make peerforge-check
```

With a TunD server already running, simulate clients without creating real TUN interfaces:

```bash
./dist/peerforge -s 127.0.0.1 -k "<network-key>" -n 253 -K 1 -d 32
```

This covers:

- registration
- encrypted authenticated keepalives
- optional client-to-client DATA probes
- rejection of clients using the wrong key through `make peerforge-wrong-key-check`

It does not test OS routing or real TUN drivers.

## Verify it works

After a successful client connection the server TUI should show peer details. Those details include virtual IP and keepalive RTT. From the client side first try:

```bash
ping 10.9.0.1
```

If ping is blocked by the OS firewall, test real TCP traffic instead. On the server:

```bash
nc -l 10.9.0.1 7777
```

On the client:

```bash
nc 10.9.0.1 7777
```

Text typed on one side should appear on the other. With two clients connected, repeat the test between their assigned `10.9.0.x` addresses to verify client-to-client relay.

## Operational checklist

1. Choose a host where UDP 9909 is reachable by all participants.
2. Ensure no participant already routes `10.9.0.0/24` through another LAN or VPN.
3. Use the same long random key on every endpoint.
4. Permit UDP 9909 in the server firewall.
5. Confirm `ping 10.9.0.1` from each client, then ping another assigned client address.
6. For games whose discovery does not cross the tunnel, enter the virtual host address manually.

If all participants are already on the same physical LAN and can reach each other directly, TunD may be unnecessary.
