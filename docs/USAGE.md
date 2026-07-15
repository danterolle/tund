# Tund — Usage

## Pre-built releases

Pre-built binaries are available on the [releases page](https://github.com/danterolle/tund/releases).

Windows releases include both `tund.exe` and `wintun.dll`; keep both files in the same folder.

## Start the server

On the machine that should act as the hub:

```bash
sudo ./tund server -k "a-long-random-key"
```

On Windows:

```powershell
.\tund.exe server -k "a-long-random-key"
```

Options:

```text
-k, --key <key>      Shared network key (same on all computers, 12+ characters)
-p, --port <port>    UDP port (default: 9909)
-t, --no-tui         Disable terminal UI (live peer dashboard)
-v, --verbose        Debug logging
```

## Connect as a client

```bash
sudo ./tund client -s <server_ip> -k "a-long-random-key"
```

On Windows:

```powershell
.\tund.exe client -s <server_ip> -k "a-long-random-key"
```

Options:

```text
-s, --server <ip>    Server IP/hostname (required)
-p, --port <port>    Server port (default: 9909)
-n, --name <name>    Display name (default: hostname)
-k, --key <key>      Shared network key (same on all computers, 12+ characters)
-t, --no-tui         Disable terminal UI (live peer dashboard)
-v, --verbose        Debug logging
```

## Example

```bash
# Machine A (server, e.g. IP 203.0.113.10):
sudo ./tund server -k "a-long-random-key"

# Machine B (client, behind NAT):
sudo ./tund client -s 203.0.113.10 -n "Gaming-PC" -k "a-long-random-key"

# Machine C (client, behind NAT):
sudo ./tund client -s 203.0.113.10 -n "Work-Laptop" -k "a-long-random-key"

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

Run `tund.exe` from the generated `dist` directory. If needed, Tund asks Windows for Administrator privileges through UAC.

## Verify it works

After a client connects, the server TUI should show the peer, its virtual IP, and an RTT value. From the client, first try:

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
