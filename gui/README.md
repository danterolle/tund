# TunD GUI

Minimal Flutter GUI for Windows/macOS/Linux.

It starts the existing `tund-cli`; it does not reimplement the tunnel. Keep the GUI and CLI together in release bundles so the launcher can find the executable.

The GUI code was built with heavier AI assistance than the C core because Flutter is not my area of expertise. The GUI remains intentionally thin: it validates inputs, launches `tund-cli`, shows logs and provides guided error messages.

## Release layout

- **Windows:** keep these release files in the same extracted folder:
  - `tund-gui.exe`
  - `tund-cli.exe`
  - `wintun.dll`
  - `flutter_windows.dll`
  - `data/`
  The GUI relaunches through UAC before it opens, so it can start `tund-cli` with `--key-stdin`.
- **macOS:** run `tund-gui.command` next to `tund-gui.app`; do not run `sudo tund-gui.app`.
- **Linux:** launch the GUI bundle with the privileges required for TUN setup. You can also run the standalone CLI with `sudo`.

The GUI warns about administrator/root privileges before starting `tund-cli`.

## Key sharing

The GUI starts `tund-cli` with `--key-stdin` so the network key is not exposed in the process list. The copied client command also uses `--key-stdin`; clients can paste or type the shared key when the command asks for it. Use `--key-file` manually if you prefer saving the key for repeated sessions.

## Bundled font

The GUI bundles Space Grotesk under the SIL Open Font License 1.1. The license text is included in `assets/fonts/OFL-SpaceGrotesk.txt`.

## Local build

```powershell
flutter build windows --release
```

```bash
flutter build macos --release
flutter build linux --release
```

Release packaging copies the CLI into the GUI bundle/folder during CI. For local manual testing, place the matching `tund-cli` binary beside the GUI executable if the launcher cannot find it.
