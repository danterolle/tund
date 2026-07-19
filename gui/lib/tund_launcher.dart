import 'dart:async';
import 'dart:io';

enum TundMode { server, client }

final _ansiEscapePattern = RegExp(
  '\u001B(?:'
  r'[@-Z\\-_]'
  r'|\[[0-?]*[ -/]*[@-~]'
  r'|\][^\u0007]*(?:\u0007|\u001B\\)'
  r')',
);

String sanitizeProcessOutput(String text) {
  return text.replaceAll(_ansiEscapePattern, '');
}

class TundConfig {
  const TundConfig({
    required this.mode,
    required this.server,
    required this.port,
    required this.name,
    required this.key,
    required this.verbose,
  });

  final TundMode mode;
  final String server;
  final String port;
  final String name;
  final String key;
  final bool verbose;

  String? validate() {
    final parsedPort = int.tryParse(port);
    if (parsedPort == null || parsedPort < 1 || parsedPort > 65535) {
      return 'Use a port between 1 and 65535.';
    }
    if (key.length < 12) {
      return 'Use a network key of at least 12 characters.';
    }
    if (mode == TundMode.client && server.isEmpty) {
      return 'Enter the server IP or hostname.';
    }
    return null;
  }

  List<String> toArgs() {
    final args = <String>[
      mode == TundMode.server ? 'server' : 'client',
      '-p',
      port,
      '-k',
      key,
      '-t',
    ];

    if (mode == TundMode.client) {
      args.addAll(['-s', server, '-n', name.isEmpty ? 'tund-client' : name]);
    }
    if (verbose) {
      args.add('-v');
    }
    return args;
  }

  String displayArgs() {
    final visible = <String>[];
    final args = toArgs();
    for (var i = 0; i < args.length; i++) {
      final value = i > 0 && args[i - 1] == '-k' ? '********' : args[i];
      visible.add(value.contains(' ') ? '"$value"' : value);
    }
    return visible.join(' ');
  }
}

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
