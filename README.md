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
- [Quick start](#quick-start)
- [Features](#features)
- [Virtual Network](#virtual-network)
- [Game compatibility](#game-compatibility)
- [Documentation](#documentation)
- [Project structure](#project-structure)
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

## Quick start

On a machine with a public IP:

```bash
sudo ./tund server -k "a-long-random-key"
```

On every machine that should join the LAN:

```bash
sudo ./tund client -s <server_ip> -k "a-long-random-key"
```

Use the same key everywhere. On Windows, run `tund.exe` normally; if needed, Tund asks for Administrator privileges through UAC.

Pre-built binaries are available on the [releases page](https://github.com/danterolle/tund/releases). For full usage, build, and verification steps, see [Usage](docs/USAGE.md).

## Features

- **Zero registration** — no accounts or external service
- **Auto-discovery** — clients are automatically recognized when they connect
- **NAT-friendly clients** — clients only need outbound UDP to a reachable server
- **Cross-platform** — Windows, macOS, and Linux
- **Single executable** — server and client modes in one program
- **Minimal dependencies** — Linux/macOS use system libraries; Windows ships with `wintun.dll`
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

## Documentation

- [Usage](docs/USAGE.md) — commands, build steps, examples, and verification.
- [Troubleshooting](docs/TROUBLESHOOTING.md) — firewall, Windows/macOS notes, and common connectivity checks.
- [Technical documentation](docs/TECHNICAL.md) — protocol, architecture, platform internals, and security boundaries.

## Project structure

Source code is organized by module under `src/`: app startup and CLI, core server/client logic, UDP networking, protocol helpers, platform TUN backends, and terminal UI. User-facing guides live in `docs/`. For implementation details, protocol framing, security boundaries, and platform lifecycle notes, see [Technical documentation](docs/TECHNICAL.md).

## Requirements

- C11 compiler (gcc, clang)
- Root/sudo (for TUN interface creation)
- Windows 10/11 x64, macOS 10.10+, or Linux 2.6+
- Windows: bundled `wintun.dll` (included in `dist/` or downloadable via `make windows`); macOS/Linux: native TUN support enabled by the OS

## License

[MIT](LICENSE)
