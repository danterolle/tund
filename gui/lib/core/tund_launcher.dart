import 'dart:async';
import 'dart:io';

import 'process_output.dart';
import 'tund_config.dart';

class TundLauncher {
  Process? _process;
  StreamSubscription<String>? _stdout;
  StreamSubscription<String>? _stderr;

  bool get running => _process != null;
  String get primaryExecutableName => executableNames.first;
  String get guiExecutableName =>
      Platform.isWindows ? 'tund-gui.exe' : 'tund-gui';
  String get missingExecutableMessage {
    if (Platform.isMacOS) {
      return 'Place $primaryExecutableName next to $guiExecutableName.app or inside $guiExecutableName.app/Contents/MacOS.';
    }
    return 'Place $primaryExecutableName in the same folder as $guiExecutableName.';
  }

  List<String> get executableNames {
    return Platform.isWindows
        ? const ['tund-cli.exe', 'tund.exe']
        : const ['tund-cli', 'tund'];
  }

  File executable() {
    for (final name in executableNames) {
      for (final directory in executableSearchDirs()) {
        final candidate =
            File('${directory.path}${Platform.pathSeparator}$name');
        if (candidate.existsSync()) {
          return candidate;
        }
      }
    }
    final fallbackDirectory = executableSearchDirs().first;
    return File(
        '${fallbackDirectory.path}${Platform.pathSeparator}$primaryExecutableName');
  }

  List<Directory> executableSearchDirs() {
    final executableDir = File(Platform.resolvedExecutable).parent;
    final dirs = <Directory>[executableDir];

    if (Platform.isMacOS &&
        executableDir.path.endsWith(
            '${Platform.pathSeparator}Contents${Platform.pathSeparator}MacOS')) {
      final appBundle = executableDir.parent.parent;
      dirs.add(appBundle.parent);
    }

    return dirs;
  }

  Future<int> start(
    TundConfig config,
    void Function(String text) onLog, {
    void Function()? onStarted,
  }) async {
    if (_process != null) {
      throw StateError('TunD is already running.');
    }

    final exe = executable();
    if (!await exe.exists()) {
      throw TundLaunchException(
        title: 'tund-cli was not found',
        message: missingExecutableMessage,
      );
    }

    if (Platform.isMacOS && !isRunningAsRoot()) {
      throw TundLaunchException(
        title: 'Administrator privileges required',
        message: macPrivilegeMessage(
          guiExecutablePath: Platform.resolvedExecutable,
          cliExecutablePath: exe.path,
        ),
      );
    }

    final process = await Process.start(
      exe.path,
      config.toArgs(),
      workingDirectory: exe.parent.path,
      runInShell: false,
    );

    _process = process;
    onStarted?.call();
    _stdout = process.stdout
        .transform(systemEncoding.decoder)
        .map(sanitizeProcessOutput)
        .listen(onLog);
    _stderr = process.stderr
        .transform(systemEncoding.decoder)
        .map(sanitizeProcessOutput)
        .listen(onLog);

    final exitCode = await process.exitCode;
    await _stdout?.cancel();
    await _stderr?.cancel();
    _stdout = null;
    _stderr = null;
    _process = null;
    return exitCode;
  }

  void stop() {
    _process?.kill();
  }

  void dispose() {
    _stdout?.cancel();
    _stderr?.cancel();
    _process?.kill();
  }
}

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

String macPrivilegeMessage({
  required String guiExecutablePath,
  required String cliExecutablePath,
}) {
  return 'TunD needs administrator privileges to create the TUN interface.\n\n'
      'On macOS, run the app executable with sudo instead of the .app bundle:\n'
      'sudo ${shellQuote(guiExecutablePath)}\n\n'
      'Or run the bundled CLI directly with sudo:\n'
      'sudo ${shellQuote(cliExecutablePath)} ...';
}

class TundLaunchException implements Exception {
  const TundLaunchException({
    required this.title,
    required this.message,
  });

  final String title;
  final String message;

  @override
  String toString() => message;
}
