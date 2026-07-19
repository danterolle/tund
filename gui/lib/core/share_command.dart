const defaultServerPlaceholder = 'SERVER_IP';

String quoteCommandArg(String value) {
  final escaped = value.replaceAll(r'\', r'\\').replaceAll('"', r'\"');
  return '"$escaped"';
}

String buildClientCommand({
  required String port,
  required String key,
  String server = defaultServerPlaceholder,
  bool maskKey = false,
}) {
  final cleanPort = port.trim().isEmpty ? '9909' : port.trim();
  final cleanKey = key.trim();
  final keyArg = maskKey
      ? quoteCommandArg('********')
      : quoteCommandArg(cleanKey.isEmpty ? '<network-key>' : cleanKey);
  return 'tund-cli client -s $server -p $cleanPort -k $keyArg';
}
