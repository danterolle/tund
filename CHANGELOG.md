# Changelog

## 3.0 - 2026-08-14

TunD 3.0 uses protocol version 5 and is not compatible with the 2.x series. Upgrade the server
and every client together.

### Added

- Paginated peer-list snapshots that cover all 253 client addresses.
- Authenticated peer-list retries when UDP pages are lost.
- Automatic client reconnection with capped exponential backoff and TUN recreation.
- Random process session IDs that isolate replay windows across restarts.

### Changed

- The authenticated datagram header now includes a 64-bit sender session ID.
- Client reconnects clear stale peer state and accept a newly assigned virtual IP.
