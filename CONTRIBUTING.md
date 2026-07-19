# Contributing to Tund

Thanks for helping improve Tund. Keep changes small and aligned with the project scope: a lightweight, self-hosted virtual IPv4 LAN for trusted groups.

## Project boundaries

- Tund is not a privacy VPN. Do not describe it as confidential until it has reviewed authenticated encryption and replay protection.
- The C core should stay dependency-light and close to the OS networking/TUN APIs.
- Platform-specific behavior should remain explicit and documented.

## Local checks

Run the smallest check that covers your change:

```bash
make test
make peerforge-check
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

## Test structure

- `tools/sitest/` contains the generic C test helper library.
- `tests/tund_test_support.*` contains Tund-specific test stubs and captures.
- `tests/test_*.c` contains unit tests for protocol and core behavior.
- `make peerforge-check` runs a local UDP integration check without real TUN interfaces.

## Commit style

Use concise conventional commit messages where possible, for example:

```text
test: cover server packet handlers
fix: correct CI macOS artifact build
```

Keep commits atomic: one logical change per commit.