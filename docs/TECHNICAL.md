# Tund — Technical documentation

## Purpose and scope

Tund is a self-hosted, hub-and-spoke virtual IPv4 LAN. It creates a local Layer-3 TUN interface on every participant and relays IP packets through one UDP server. It is intended for small, trusted groups and games that work over IPv4, such as Artemis Space Ship Simulator.

It is not an Ethernet bridge, a general-purpose privacy VPN, or a replacement for a production VPN. It does not transport Ethernet frames, IPv6, or arbitrary multicast discovery.

## Components

| Component | Responsibility |
|---|---|
| `src/app/` | Entry point, CLI parsing, logging, platform startup, Windows elevation, common types, global state. |
| `src/protocol/` | Datagram framing, message serialisation, virtual-network constants and SipHash MAC. |
| `src/net/` | UDP sockets, hostname resolution, packet authentication, source-address comparison. |
| `src/core/` | Core server/client state machines and shared orchestration. |
| `src/core/server/` | Server peer table, packet handlers, timeout/TUN threads, lifecycle and logging. |
| `src/core/client/` | Client registration, peer table, server message handlers, logging, and tunnel lifecycle. |
| `src/tun/` | Cross-platform TUN API plus Linux, macOS, and Windows platform implementations. |
| `src/tun/linux/` | Linux `/dev/net/tun` implementation. |
| `src/tun/darwin/` | macOS `utun` device and interface configuration. |
| `src/tun/windows/` | Windows Wintun loading, process helpers, and IP/route/MTU configuration. |
| `src/ui/` | Public TUI API, rendering helpers, and recent event log. |
| [`gui/`](../gui/README.md) | Optional Flutter Windows launcher that starts `tund-cli.exe` without reimplementing the tunnel. |

## Virtual network

The virtual network is fixed to `10.9.0.0/24`.

- `10.9.0.1`: hub server
- `10.9.0.2`–`10.9.0.254`: clients, assigned sequentially from the first free address
- `10.9.0.255`: IPv4 broadcast

Each endpoint sets the TUN MTU to 1400 bytes. The lower-than-Ethernet MTU reserves space for UDP encapsulation and limits normal fragmentation.

## Start-up sequence

1. The user selects server/client and supplies the shared network key.
2. `main.c` rejects keys shorter than 12 characters and derives the two SipHash key words.
3. A server binds UDP port 9909 by default, opens a TUN device and assigns `10.9.0.1/24`.
4. A client resolves the server, binds an ephemeral UDP port and sends `MSG_REGISTER`.
5. The server allocates an unused `10.9.0.x` address and replies with `MSG_ASSIGN`.
6. The client configures its TUN interface and starts the TUN-reader and keepalive threads.
7. Both sides exchange keepalive probes and acknowledgements to update liveness and keepalive RTT.

## Datagram format

Every Tund datagram uses the following 13-byte header followed by a payload:

| Bytes | Field | Description |
|---:|---|---|
| 0 | magic | Fixed value `0xA9`. |
| 1 | version | Protocol version. |
| 2 | type | Message type. |
| 3–4 | length | Big-endian payload length. |
| 5–12 | tag | 64-bit SipHash integrity tag. |

The tag covers the stable header fields and payload. `network.c` signs every outgoing packet and drops any incoming packet with an invalid tag before message parsing. All participants must therefore use the same access key and protocol version.

Message types are `REGISTER`, `ASSIGN`, `PEER_LIST`, `DATA`, `KEEPALIVE`, `KEEPALIVE_ACK`, `DISCONNECT`, `PEER_JOIN`, and `PEER_LEAVE`.

## Keepalive and RTT

`KEEPALIVE` carries the sender's monotonic millisecond timestamp. The receiver replies with `KEEPALIVE_ACK` containing the same timestamp. The original sender compares it with its current monotonic time to estimate RTT.

Clients periodically probe the server and display smoothed keepalive RTT in the TUI. The server also probes each active peer, updates `last_seen`, and records smoothed per-peer keepalive RTT for the peer table. Tund uses a simple EWMA (`7/8` previous value, `1/8` latest sample) to hide scheduler spikes while still following real changes. This is an application-level health metric, not an ICMP ping measurement.

## Packet forwarding

### Client → server

The client TUN thread reads an IPv4 packet, wraps it as `MSG_DATA`, signs it and sends it to the configured server. The server accepts data only if the UDP source endpoint belongs to an active peer.

### Server → client

The hub reads the destination IPv4 address at offset 16 of the IP header:

- traffic for `10.9.0.1` is written to the server TUN;
- traffic for `10.9.0.255` is relayed to every peer except the sender and to the server TUN;
- traffic for an assigned client address is relayed to that endpoint.

Clients accept traffic only when its UDP source address equals the configured server address. This prevents arbitrary local UDP packets from being injected into the virtual interface.

## Concurrency and shutdown

The server has a timeout thread and a TUN-reader thread. Each client has a keepalive thread and a TUN-reader thread. Peer-table access is protected by a mutex. UI state for the TUI is copied under the same peer lock.

On shutdown, Tund sets stop flags, sends disconnect notifications where possible, and closes the TUN before joining its reader thread. Linux and macOS TUN reads are guarded by short `poll()` timeouts so Ctrl+C remains responsive even when closing the descriptor does not immediately unblock a read. Windows waits on Wintun's read event with a bounded timeout.

## Security model

The shared key authenticates datagrams and prevents unauthorised endpoints without the key from registering or injecting packets. It does **not** encrypt packet contents and it does not provide replay protection. Use a long random key and only trusted networks/servers.

Do not describe Tund as a confidential VPN until it uses a reviewed authenticated-encryption transport and replay protection.

## Platform notes

- **Linux:** requires `/dev/net/tun` and root/CAP_NET_ADMIN.
- **macOS:** uses an OS-managed `utun` interface configured via `ioctl(SIOCAIFADDR)` and `ioctl(SIOCSIFMTU)`; the subnet route is added through `fork`+`exec(/sbin/route)` — no shell processes are spawned. Run with administrator rights.
- **Windows:** requires Administrator privileges and `wintun.dll` beside the executable. If started without elevation, Tund relaunches itself through UAC with the same arguments. Tund uses `netsh` for adapter IP/MTU configuration, creates the tunnel route with IP Helper APIs, then verifies IP address, route, and MTU with IP Helper APIs. It does not modify Windows Firewall automatically.
