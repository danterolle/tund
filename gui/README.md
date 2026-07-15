# Tund GUI

Minimal Flutter launcher for Windows 10/11.

It starts the existing `tund.exe`; it does not reimplement the tunnel.

## Build

```powershell
flutter create --platforms=windows --project-name tund_gui .
flutter build windows --release
```

Keep `tund_gui.exe`, `flutter_windows.dll`, the Flutter `data` folder, `tund.exe`, and `wintun.dll` together in the same folder.
