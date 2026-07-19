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
      throw StateError('Tund is already running.');
    }

    final exe = executable();
    if (!await exe.exists()) {
      throw TundLaunchException(missingExecutableMessage);
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

class TundLaunchException implements Exception {
  const TundLaunchException(this.message);

  final String message;

  @override
  String toString() => message;
}
