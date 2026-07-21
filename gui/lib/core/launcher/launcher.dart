import 'dart:async';
import 'dart:convert';
import 'dart:io';

import '../config.dart';
import '../process_output.dart';

part 'exceptions.dart';
part 'macos.dart';

class TundLauncher {
  static const _stopTimeout = Duration(seconds: 3);

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
    void Function(String line)? onJsonEvent,
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
      final commandPath = macGuiCommandPath(Platform.resolvedExecutable);
      throw TundLaunchException(
        title: 'Administrator privileges required',
        message: macPrivilegeMessage(
          guiExecutablePath: Platform.resolvedExecutable,
          cliExecutablePath: exe.path,
          commandPath: commandPath.isNotEmpty && File(commandPath).existsSync()
              ? commandPath
              : null,
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
    if (onJsonEvent != null) {
      _stdout = process.stdout
          .transform(tundProcessOutputDecoder)
          .transform(const LineSplitter())
          .listen(onJsonEvent);
    } else {
      _stdout = process.stdout
          .transform(tundProcessOutputDecoder)
          .map(sanitizeProcessOutput)
          .listen(onLog);
    }
    _stderr = process.stderr
        .transform(tundProcessOutputDecoder)
        .map(sanitizeProcessOutput)
        .listen(onLog);

    try {
      process.stdin.writeln(config.key);
      await process.stdin.flush();
    } on IOException {
      process.kill();
      throw const TundLaunchException(
        title: 'Could not send network key',
        message:
            'The CLI process exited before the GUI could send the network key.',
      );
    } on StateError {
      process.kill();
      throw const TundLaunchException(
        title: 'Could not send network key',
        message:
            'The CLI process closed stdin before the GUI could send the network key.',
      );
    }

    onStarted?.call();

    final exitCode = await process.exitCode;
    await _stdout?.cancel();
    await _stderr?.cancel();
    _stdout = null;
    _stderr = null;
    _process = null;
    return exitCode;
  }

  void stop() {
    final process = _process;
    if (process == null) return;
    if (!_requestGracefulStop(process)) return;
    unawaited(_forceKillIfStillRunning(process));
  }

  void dispose() {
    _stdout?.cancel();
    _stderr?.cancel();
    stop();
  }

  Future<void> _forceKillIfStillRunning(Process process) async {
    final exited = await process.exitCode
        .then((_) => true)
        .timeout(_stopTimeout, onTimeout: () => false);
    if (!exited && _process == process) {
      process.kill(ProcessSignal.sigkill);
    }
  }

  bool _requestGracefulStop(Process process) {
    try {
      process.stdin.writeln('stop');
    } on IOException {
      process.kill();
      return false;
    } on StateError {
      process.kill();
      return false;
    }
    unawaited(_flushStopCommand(process));
    return true;
  }

  Future<void> _flushStopCommand(Process process) async {
    try {
      await process.stdin.flush();
    } on IOException {
      process.kill();
    } on StateError {
      process.kill();
    }
  }
}
