<p align="center">
  <img src="logo.svg" width="120" alt="Tund logo">
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="MIT"></a>
  <a href="https://github.com/danterolle/tund/releases"><img src="https://img.shields.io/github/v/release/danterolle/tund" alt="Release"></a>
  <img src="https://img.shields.io/badge/OS-Windows%20%7C%20Linux%20%7C%20macOS-378ADD" alt="Platform">
  <img src="https://img.shields.io/badge/language-C-555555.svg" alt="Language">
</p>

# Tund

**A lightweight, self-hosted virtual IPv4 LAN.**

Does one thing: create a virtual LAN. Designed for Artemis (the Spaceship Bridge Simulator) LAN parties and direct-IP multiplayer with friends.

Built iteratively with DeepSeek V4 Flash: observing the generated code, refactoring and modularising as it grew. A "one-shot" tool made for fun, for a specific purpose and perhaps to learn something along the way.

It behaves like Radmin VPN, but open source and cross-platform. Written in C — Linux and macOS use only system libraries; Windows bundles Wintun for TUN support.

## Table of Contents

- [How it works](#how-it-works)
- [Usage](#usage)
  - [Start the server](#start-the-server)
  - [Connect as a client](#connect-as-a-client)
  - [Example](#example)
- [Build](#build)
  - [Linux / macOS](#linux--macos)
  - [Windows](#windows)
- [Features](#features)
- [Virtual Network](#virtual-network)
- [Game compatibility](#game-compatibility)
- [Network details and troubleshooting](#network-details-and-troubleshooting)
  - [Windows notes](#windows-notes)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [License](#license)

## How it works

```
┌──────────┐         ┌─────────────┐         ┌──────────┐
│ Client A │ ──UDP──▶│  Tund Hub   │◀──UDP── │ Client B │
│ 10.9.0.2 │         │  10.9.0.1   │         │ 10.9.0.3 │
└──────────┘         └─────────────┘         └──────────┘
     ▲                                              ▲
     └──────────── Virtual LAN: 10.9.0.0/24 ────────┘
```

1. One machine runs Tund in **server** mode. On the same physical LAN it only needs to be reachable by the other PCs; over the Internet it needs a reachable UDP port.
2. Other machines connect as **clients**
3. All clients automatically discover each other
4. Traffic is tunneled over UDP through the server
5. Each client gets a virtual IP in the `10.9.0.0/24` range

## Usage

### Start the server

On a machine with a public IP:

```bash
sudo ./tund server -k "a-long-random-key"
```

Options:
```
-k, --key <key>      Shared network key (same on all computers, 12+ characters)
-p, --port <port>    UDP port (default: 9909)
-t, --no-tui         Disable terminal UI (live peer dashboard)
-v, --verbose        Debug logging
```

### Connect as a client

```bash
sudo ./tund client -s <server_ip> -k "a-long-random-key"
```

Options:
```
-s, --server <ip>    Server IP/hostname (required)
-p, --port <port>    Server port (default: 9909)
-n, --name <name>    Display name (default: hostname)
-k, --key <key>      Shared network key (same on all computers, 12+ characters)
-t, --no-tui         Disable terminal UI (live peer dashboard)
-v, --verbose        Debug logging
```

### Example

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

Pre-built binaries are available on the [releases page](https://github.com/danterolle/tund/releases).

### Linux / macOS

```bash
make
```

### Windows

```bash
make windows
```

Run `tund.exe` from the generated `dist` directory. If needed, Tund asks Windows for Administrator privileges through UAC. Keep `wintun.dll` next to the executable.

Or download the latest release — `tund.exe` and `wintun.dll` are already bundled together.

## Features

- **Zero registration** — no accounts or external service
- **Auto-discovery** — clients are automatically recognized when they connect
- **NAT traversal** — works behind any type of NAT (relay mode)
- **Cross-platform** — Windows, macOS, and Linux
- **Single binary** — one executable, server and client in one
- **No external dependencies** — pure C, only uses system libraries
- **Graceful shutdown** — Ctrl+C cleanly disconnects and notifies peers
- **Keepalive** — automatic peer timeout detection (30s) and RTT tracking
- **Broadcast support** — LAN broadcast packets are forwarded
- **Authenticated membership** — packets without the shared network key are discarded

## Virtual Network

| Address | Role |
|---------|------|
| `10.9.0.1` | Server |
| `10.9.0.2` - `10.9.0.254` | Clients (auto-assigned) |
| `10.9.0.255` | Broadcast |

Maximum 253 simultaneous clients per network.

## Game compatibility

Tund transports IPv4 traffic (including the IPv4 subnet broadcast address). It is suitable for direct-IP games and games such as Artemis that use ordinary IPv4 networking. It is not an Ethernet bridge: games requiring Layer-2 discovery, IPv6, or multicast discovery may need direct IP entry or may not work.

The shared key authenticates packets but does **not** encrypt traffic. Tund is not a production VPN — it is a weekend project for trusted networks, not a privacy tool. Do not use on untrusted networks or untrusted servers.

For Artemis, start the server first, connect every station with the same key, then use the assigned `10.9.0.x` addresses where the game asks for a host address. Verify first with `ping 10.9.0.1` from each client. If automatic discovery does not appear, prefer entering the host IP manually.

## Network details and troubleshooting

- Tund listens on UDP port `9909` by default. Allow inbound UDP on the server firewall; clients normally need only outbound UDP.
  - **Linux (ufw):** `sudo ufw allow 9909/udp`
  - **Linux (firewalld):** `sudo firewall-cmd --add-port=9909/udp --permanent && sudo firewall-cmd --reload`
  - **Linux (iptables):** `sudo iptables -A INPUT -p udp --dport 9909 -j ACCEPT`
  - **Windows:** `netsh advfirewall firewall add rule name="Tund" dir=in action=allow protocol=udp localport=9909`
  - **macOS:** usually no firewall blocks inbound by default; check System Settings → Network → Firewall
- The tunnel MTU is `1400` bytes, leaving room for UDP encapsulation.
- Each datagram has a 13-byte Tund header: magic/version/type/length plus an 8-byte SipHash integrity tag derived from the shared key. A mismatched key appears as a timeout by design.
- The virtual subnet is fixed at `10.9.0.0/24`. Do not use Tund on a host already routing that subnet through a real LAN or another VPN.
- All participants must run the same Tund protocol version: authenticated framing is not compatible with older builds.
- To test connectivity, `ping 10.9.0.1` from each client after connecting. If ping fails despite a successful registration (the client gets a virtual IP), the server firewall is likely blocking ICMP.

### Windows notes

- **Administrator privileges**: Tund creates a TUN interface, so Windows may show a UAC prompt on startup. Accept it to continue.

- **Server firewall**: Tund does not change Windows Firewall automatically. If this machine is the server, allow inbound UDP on port `9909` or clients will not be able to reach it.
  ```cmd
  :: Persistent rule (recommended)
  netsh advfirewall firewall add rule name="Tund" dir=in action=allow protocol=udp localport=9909

  :: Temporarily disable (test only)
  netsh advfirewall set allprofiles state off
  netsh advfirewall set allprofiles state on
  ```

- **ICMP (ping)**: Windows Firewall often blocks ICMP echo requests on the virtual adapter. After verifying connectivity by temporarily disabling the firewall (above), re-enable it. Ping is not required for game traffic, only for connectivity checks.

- **Console encoding**: `tund.exe` (the console build) sets UTF-8 automatically. If characters display incorrectly in older terminals, run `chcp 65001` before launching the program.

### macOS notes

- **Unverified developer warning**: the first time you run Tund, macOS may show *"apple cannot verify the identity of the developer"*. This happens because the binary is not signed with an Apple Developer certificate. To bypass:
  ```bash
  xattr -d com.apple.quarantine ./tund
  ```
  Run this once before launching. If the warning persists, open **System Settings → Privacy & Security** and click *Open Anyway* next to the blocked Tund entry.

- **TUN interface**: macOS creates a `utun` device automatically when Tund starts. You may see a prompt asking for network configuration permission — enter your password to allow it.

- **ICMP (ping)**: like Windows, macOS may block ICMP on the virtual interface. Ping is not required for game traffic.

## Architecture

```
tund
├── src/
│   ├── main.c          # Entry point, CLI parsing
│   ├── tund.h         # Common types, logging
│   ├── protocol.h      # Wire protocol (UDP messages)
│   ├── tun.h           # TUN interface API
│   ├── tun_linux.c     # Linux TUN implementation
│   ├── tun_darwin.c    # macOS utun implementation
│   ├── tun_windows.c   # Windows TUN implementation (Wintun)
│   ├── network.h       # UDP socket API
│   ├── network.c       # Socket operations
│   ├── server.h        # Server API
│   ├── server.c        # Hub server (relay + peer mgmt)
│   ├── client.h        # Client API
│   ├── client.c        # Client (TUN + tunnel)
│   ├── tui.h           # Terminal UI header
│   ├── tui.c           # Terminal UI (live peer dashboard)
│   └── wintun.h        # Wintun API declarations
├── Makefile
└── README.md
```

For implementation details, protocol framing, security boundaries, and platform lifecycle notes, see [Technical documentation](docs/TECHNICAL.md).

## Requirements

- C11 compiler (gcc, clang)
- Root/sudo (for TUN interface creation)
- Windows 10/11 x64, macOS 10.10+, or Linux 2.6+
- Windows: bundled `wintun.dll` (included in `dist/` or downloadable via `make windows`); macOS/Linux: native TUN support enabled by the OS

## License

[MIT](LICENSE)
