class GuidedError {
  const GuidedError({required this.title, required this.message});

  final String title;
  final String message;
}

class ErrorRule {
  const ErrorRule({
    required this.needles,
    required this.error,
  });

  final List<String> needles;
  final GuidedError error;

  bool matches(String output) {
    for (final needle in needles) {
      if (output.contains(needle)) return true;
    }
    return false;
  }
}

const fallbackGuidance = GuidedError(
  title: 'TunD stopped with an error',
  message: 'Check the raw logs below for the exact tund-cli output.',
);

const errorRules = [
  ErrorRule(
    needles: ['wintun.dll', 'wintun'],
    error: GuidedError(
      title: 'Wintun is missing or unavailable',
      message:
          'Keep wintun.dll in the same folder as tund-cli.exe and tund-gui.exe. Use the official Windows release zip if possible.',
    ),
  ),
  ErrorRule(
    needles: [
      'administrator privileges',
      'root privileges',
      'cap_net_admin',
      '/dev/net/tun',
      'operation not permitted',
      'permission denied',
    ],
    error: GuidedError(
      title: 'Missing privileges',
      message:
          'TunD needs administrator/root privileges to create and configure the virtual network interface.',
    ),
  ),
  ErrorRule(
    needles: [
      'cannot bind udp port',
      'address already in use',
      'choose a different port',
    ],
    error: GuidedError(
      title: 'UDP port is already in use',
      message:
          'Stop the other TunD/server process or choose a different UDP port.',
    ),
  ),
  ErrorRule(
    needles: [
      'authentication failed',
      'shared key',
      'shared access key',
      'network key',
    ],
    error: GuidedError(
      title: 'Network key mismatch',
      message: 'Use the exact same shared network key on every TunD endpoint.',
    ),
  ),
  ErrorRule(
    needles: [
      'registration timed out',
      'no response from server',
      'cannot resolve server',
    ],
    error: GuidedError(
      title: 'Cannot reach the server',
      message:
          'Check the server address, UDP port, firewall rules, and that the TunD server is running.',
    ),
  ),
  ErrorRule(
    needles: [
      '10.9.0.0/24 conflicts',
      'another vpn',
      'route/subnet',
      'route ',
    ],
    error: GuidedError(
      title: 'Route or subnet conflict',
      message:
          'Another VPN or LAN may already use 10.9.0.0/24. Disconnect it before starting TunD.',
    ),
  ),
];

GuidedError guideTundFailure(String output) {
  final text = output.toLowerCase();
  for (final rule in errorRules) {
    if (rule.matches(text)) return rule.error;
  }
  return fallbackGuidance;
}
