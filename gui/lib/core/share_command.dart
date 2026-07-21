const defaultServerPlaceholder = 'SERVER_IP';

String buildClientCommand({
  required String port,
  String server = defaultServerPlaceholder,
}) {
  final cleanPort = port.trim().isEmpty ? '9909' : port.trim();
  return 'tund-cli client -s $server -p $cleanPort --key-stdin';
}
