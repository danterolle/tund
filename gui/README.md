# Tund GUI

Minimal Flutter GUI for Windows 10/11.

It starts the existing `tund-cli.exe`; it does not reimplement the tunnel.

This GUI exists because a friend explicitly asked for a Windows-friendly way to use Tund without opening a single terminal window. I am not familiar with Flutter or Dart so this code was primarily produced by AI under my supervision and practical engineering experience.

## Local build

```powershell
flutter build windows --release
```

Keep `tund-gui.exe`, `flutter_windows.dll`, the Flutter `data` folder, `tund-cli.exe`, and `wintun.dll` together in the same folder.
