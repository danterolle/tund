# Contributing to TunD

Thanks for helping improve TunD. Keep changes small and aligned with the project scope: a lightweight, self-hosted virtual IPv4 LAN for trusted groups.

## Project boundaries

- TunD is not a general privacy VPN. Transport encryption exists, but traffic is not end-to-end encrypted against the server.
- Replay protection exists for TunD datagrams.
- The C core should stay dependency-light and close to the OS networking/TUN APIs.
- Platform-specific behavior should remain explicit and documented.

## Local checks

Run the smallest check that covers your change:

```bash
make test
make peerforge-check
make peerforge-wrong-key-check
make sanitize
make tools
make all
```

For a broader local verification:

```bash
make verify
```

`make verify` also runs the Windows cross-build, so it needs mingw-w64 and may need network access to download `wintun.dll`.

For GUI changes:

```bash
cd gui
flutter pub get
flutter analyze
flutter test
```

## Documentation and website

- Keep the security model wording consistent. TunD datagrams are encrypted in transit and replay-protected. Traffic is not end-to-end encrypted against the server.
- Website sources live in `web/` and are published by the `Pages` workflow as the GitHub Pages site root.
- Release steps are documented in [`docs/RELEASE.md`](docs/RELEASE.md).

## Test structure

- `tools/sitest/` contains the generic C test helper library.
- `tests/tund_test_support.*` contains TunD-specific test stubs and captures.
- `tests/test_*.c` contains unit tests for protocol and core behavior.
- `make peerforge-check` runs a local UDP integration check without real TUN interfaces.

## Commit style

Use concise conventional commit messages where possible, for example:

```text
test: cover server packet handlers
fix: correct CI macOS artifact build
```

Keep commits atomic: one logical change per commit.