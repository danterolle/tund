enum TundMode { server, client }

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
