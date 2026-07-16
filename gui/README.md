# Tund GUI

Minimal Flutter GUI for Windows, macOS, and Linux.

It starts the existing `tund-cli`; it does not reimplement the tunnel.

This GUI exists because a friend explicitly asked for a Windows-friendly way to use Tund without opening a single terminal window. I am not familiar with Flutter or Dart so this code was primarily produced by AI under my supervision and practical engineering experience.

## Local build

```powershell
flutter build windows --release
```

```bash
flutter build macos --release
flutter build linux --release
```

The GUI starts `tund-cli` from its bundle/folder and warns before launch that TUN setup requires Administrator/root privileges. On Windows the runner asks for UAC elevation; on macOS and Linux, launch the GUI with the required privileges or run `tund-cli` directly with `sudo` if startup fails.
