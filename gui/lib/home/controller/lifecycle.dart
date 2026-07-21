part of 'home_controller.dart';

Future<void> _startTund(
  TundHomeController controller, {
  required Future<bool> Function() confirmPrivileges,
  required void Function(String message) showError,
}) async {
  if (controller.running) return;

  final config = controller.currentConfig();
  final error = config.validate();
  if (error != null) {
    showError(error);
    return;
  }
  if (!await confirmPrivileges() || controller.disposed) return;

  controller.prepareStart(config);

  try {
    final exitCode = await controller.launcher.start(
      config,
      controller.appendLog,
      onJsonEvent: controller.applyJsonEvent,
      onStarted: controller.markRunning,
    );
    controller.completeExit(exitCode);
  } on TundLaunchException catch (error) {
    controller.failWithGuidance(error.title, error.message);
    showError(error.message);
  } catch (error) {
    controller.failWithGuidance(
      'Could not start tund-cli',
      'The process could not be launched. Check the app folder and raw logs.',
    );
    showError(
        'Cannot start ${controller.launcher.primaryExecutableName}: $error');
  }
}

void _prepareStart(TundHomeController controller, TundConfig config) {
  controller.status = GuiStatus.starting;
  controller.guidedError = null;
  controller.stopRequested = false;
  controller.hostPeerTracker.clear();
  controller.hostPeers = const [];
  controller.log.clear();
  controller.notifyListeners();
  controller.appendLog(
    '> ${controller.launcher.executable().uri.pathSegments.last} ${config.displayArgs()}\n\n',
  );
}

void _markRunning(TundHomeController controller) {
  if (controller.disposed) return;
  controller.status = GuiStatus.running;
  controller.notifyListeners();
}

void _completeExit(TundHomeController controller, int exitCode) {
  if (controller.disposed) return;
  final stoppedByUser = controller.stopRequested;
  controller.status = statusForTundExit(exitCode, stopRequested: stoppedByUser);
  controller.guidedError = guidedErrorForTundExit(
    exitCode,
    stopRequested: stoppedByUser,
    output: controller.log.toString(),
  );
  controller.stopRequested = false;
  controller.clearNetworkKey();
  controller.notifyListeners();
  controller.appendLog(
    stoppedByUser
        ? '\nTunD stopped.\n'
        : '\nTunD exited with code $exitCode.\n',
  );
}

void _failWithGuidance(
  TundHomeController controller,
  String title,
  String message,
) {
  if (controller.disposed) return;
  controller.status = GuiStatus.failed;
  controller.stopRequested = false;
  controller.guidedError = GuidedError(title: title, message: message);
  controller.clearNetworkKey();
  controller.notifyListeners();
}

void _stopTund(TundHomeController controller) {
  controller.status = GuiStatus.stopping;
  controller.stopRequested = true;
  controller.guidedError = null;
  controller.notifyListeners();
  controller.appendLog('\nStopping TunD...\n');
  controller.launcher.stop();
}
