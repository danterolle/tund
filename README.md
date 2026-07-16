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

It behaves like Radmin VPN, but open source and cross-platform. The core is written in C — Linux and macOS use only system libraries; Windows bundles Wintun for TUN support.

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
sudo ./tund-cli server -k "a-long-random-key"
```

On every machine that should join the LAN:

```bash
sudo ./tund-cli client -s <server_ip> -k "a-long-random-key"
```

Use the same key everywhere. On desktop releases, run `tund-gui` for the GUI or `tund-cli` for the CLI; if needed, the CLI still requires administrator/root privileges for TUN setup. See the [GUI README](gui/README.md) for details.

Pre-built binaries are available on the [releases page](https://github.com/danterolle/tund/releases). For full usage, build, and verification steps, see [Usage](docs/USAGE.md).

## Features

- **Zero registration** — no accounts or external service
- **NAT-friendly clients** — clients only need outbound UDP to a reachable server
- **Cross-platform** — Windows, macOS, and Linux
- **Desktop GUI** — optional Flutter launcher for non-terminal users, documented in [`gui/README.md`](gui/README.md)
- **Single CLI binary** — server and client modes in one program
- **IPv4 broadcast support** — useful for LAN-style discovery in compatible games
- **Authenticated membership** — packets without the shared network key are discarded

## Game compatibility

Tund transports IPv4 traffic (including the IPv4 subnet broadcast address). It is suitable for direct-IP games and games such as Artemis that use ordinary IPv4 networking. It is not an Ethernet bridge: games requiring Layer-2 discovery, IPv6, or multicast discovery may need direct IP entry or may not work.

The shared key authenticates packets but does **not** encrypt traffic. Tund is not a production VPN — it is a weekend project for trusted networks, not a privacy tool. Do not use on untrusted networks or untrusted servers.

For Artemis, start the server first, connect every station with the same key, then use the assigned `10.9.0.x` addresses where the game asks for a host address. Verify first with `ping 10.9.0.1` from each client. If automatic discovery does not appear, prefer entering the host IP manually.

## Documentation

- [Usage](docs/USAGE.md) — commands, build steps, examples, and verification.
- [Troubleshooting](docs/TROUBLESHOOTING.md) — firewall, Windows/macOS notes, and common connectivity checks.
- [Technical documentation](docs/TECHNICAL.md) — protocol, architecture, platform internals, and security boundaries.
- [GUI README](gui/README.md) — Windows Flutter launcher notes and build details.

## Requirements

- C11 compiler (gcc, clang)
- Root/sudo (for TUN interface creation)
- Windows 10/11 x64, macOS 10.10+, or Linux 2.6+
- Release builds include `tund-cli` and optional GUI bundles; Windows also ships `wintun.dll`

## License

[MIT](LICENSE)
