# TunD GUI

Minimal Flutter GUI for Windows, macOS, and Linux.

It starts the existing `tund-cli`; it does not reimplement the tunnel. Keep the GUI and CLI together in release bundles so the launcher can find the executable.

## Release layout

- **Windows:** `tund-gui.exe`, `tund-cli.exe`, `wintun.dll`, `flutter_windows.dll`, and the `data/` directory must stay in the same extracted folder.
- **macOS:** run `tund-gui.command` next to `tund-gui.app`; do not run `sudo tund-gui.app`.
- **Linux:** launch the GUI bundle with the privileges required for TUN setup, or run the standalone CLI with `sudo`.

The GUI warns about administrator/root privileges before starting `tund-cli`.

## Local build

```powershell
flutter build windows --release
```

```bash
flutter build macos --release
flutter build linux --release
```

Release packaging copies the CLI into the GUI bundle/folder during CI. For local manual testing, place the matching `tund-cli` binary beside the GUI executable if the launcher cannot find it.
