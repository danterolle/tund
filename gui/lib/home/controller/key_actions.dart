part of 'controller.dart';

void _generateKey(TundHomeController controller) {
  controller.key.text = generateNetworkKey();
  controller.showKey = true;
  controller.notifyListeners();
}

Future<void> _copyKey(
  TundHomeController controller, {
  required Future<void> Function(String text) copyText,
  required void Function(String message) showError,
  required void Function(String message) showInfo,
}) async {
  final value = controller.key.text.trim();
  if (value.isEmpty) {
    showError('Enter or generate a network key first.');
    return;
  }

  await copyText(value);
  showInfo('Network key copied. Clear your clipboard after sharing it.');
}

Future<void> _copyClientCommand(
  TundHomeController controller, {
  required Future<void> Function(String text) copyText,
  required void Function(String message) showInfo,
}) async {
  await copyText(buildClientCommand(port: controller.port.text));
  showInfo('Client command copied. Paste the network key when prompted.');
}

void _toggleKeyVisibility(TundHomeController controller) {
  controller.showKey = !controller.showKey;
  controller.notifyListeners();
}

void _clearNetworkKey(TundHomeController controller) {
  controller.key.clear();
  controller.showKey = false;
}
