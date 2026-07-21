part of 'launcher.dart';

bool isRunningAsRoot() {
  if (!Platform.isMacOS && !Platform.isLinux) {
    return false;
  }
  final result = Process.runSync('/usr/bin/id', ['-u'], runInShell: false);
  return result.exitCode == 0 && result.stdout.toString().trim() == '0';
}

String shellQuote(String value) {
  return "'${value.replaceAll("'", "'\"'\"'")}'";
}

String macGuiCommandPath(String guiExecutablePath) {
  const bundleMarker = '.app/Contents/MacOS/';
  final markerIndex = guiExecutablePath.indexOf(bundleMarker);
  if (markerIndex < 0) {
    return '';
  }

  final bundlePath =
      guiExecutablePath.substring(0, markerIndex + '.app'.length);
  final bundleParentEnd = bundlePath.lastIndexOf('/');
  if (bundleParentEnd < 0) {
    return '';
  }
  return '${bundlePath.substring(0, bundleParentEnd + 1)}tund-gui.command';
}

String macPrivilegeMessage({
  required String guiExecutablePath,
  required String cliExecutablePath,
  String? commandPath,
}) {
  if (commandPath != null) {
    return 'TunD needs administrator privileges to create the TUN interface.\n\n'
        'Open the launcher next to tund-gui.app, or run it from Terminal:\n'
        '${shellQuote(commandPath)}\n\n'
        'Do not run sudo tund-gui.app; that points to the app bundle directory.';
  }

  return 'TunD needs administrator privileges to create the TUN interface.\n\n'
      'On macOS, run the app executable with sudo instead of the .app bundle:\n'
      'sudo ${shellQuote(guiExecutablePath)}\n\n'
      'Or run the bundled CLI directly with sudo:\n'
      'sudo ${shellQuote(cliExecutablePath)} ...';
}
