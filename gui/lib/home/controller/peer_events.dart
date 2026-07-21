part of 'home_controller.dart';

void _applyJsonEvent(TundHomeController controller, String line) {
  if (controller.disposed || controller.mode != TundMode.server) return;
  try {
    if (controller.hostPeerTracker.applyJsonLine(line)) {
      controller.hostPeers = controller.hostPeerTracker.peers;
    }
  } on FormatException {
    controller.appendLog('$line\n');
    return;
  }
  controller.notifyListeners();
}
