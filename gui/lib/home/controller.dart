import 'dart:io';

import 'package:flutter/material.dart';

import '../core/error_guidance.dart';
import '../core/host_peer.dart';
import '../core/key_helpers.dart';
import '../core/share_command.dart';
import '../core/status.dart';
import '../core/tund_config.dart';
import '../core/tund_launcher.dart';

class TundHomeController extends ChangeNotifier {
  final server = TextEditingController();
  final port = TextEditingController(text: '9909');
  final name = TextEditingController(text: Platform.localHostname);
  final key = TextEditingController();
  final log = StringBuffer();
  final logScroll = ScrollController();
  final launcher = TundLauncher();
  final hostPeerTracker = HostPeerTracker();

  TundMode mode = TundMode.server;
  List<HostPeer> hostPeers = const [];
  bool verbose = false;
  bool showKey = false;
  bool privilegeNoticeAccepted = false;
  bool stopRequested = false;
  GuiStatus status = GuiStatus.ready;
  GuidedError? guidedError;
  bool disposed = false;

  bool get running => status == GuiStatus.starting || launcher.running;

  @override
  void dispose() {
    disposed = true;
    launcher.dispose();
    server.dispose();
    port.dispose();
    name.dispose();
    key.dispose();
    logScroll.dispose();
    super.dispose();
  }

  @override
  void notifyListeners() {
    if (!disposed) super.notifyListeners();
  }

  Future<void> startTund({
    required Future<bool> Function() confirmPrivileges,
    required void Function(String message) showError,
  }) async {
    if (running) return;

    final config = currentConfig();
    final error = config.validate();
    if (error != null) {
      showError(error);
      return;
    }
    if (!await confirmPrivileges() || disposed) return;

    prepareStart(config);

    try {
      final exitCode = await launcher.start(
        config,
        appendLog,
        onJsonEvent: applyJsonEvent,
        onStarted: markRunning,
      );
      completeExit(exitCode);
    } on TundLaunchException catch (error) {
      failWithGuidance(error.title, error.message);
      showError(error.message);
    } catch (error) {
      failWithGuidance(
        'Could not start tund-cli',
        'The process could not be launched. Check the app folder and raw logs.',
      );
      showError('Cannot start ${launcher.primaryExecutableName}: $error');
    }
  }

  void prepareStart(TundConfig config) {
    status = GuiStatus.starting;
    guidedError = null;
    stopRequested = false;
    hostPeerTracker.clear();
    hostPeers = const [];
    log.clear();
    notifyListeners();
    appendLog(
      '> ${launcher.executable().uri.pathSegments.last} ${config.displayArgs()}\n\n',
    );
  }

  void markRunning() {
    if (disposed) return;
    status = GuiStatus.running;
    notifyListeners();
  }

  void completeExit(int exitCode) {
    if (disposed) return;
    final stoppedByUser = stopRequested;
    status = statusForTundExit(exitCode, stopRequested: stoppedByUser);
    guidedError = guidedErrorForTundExit(
      exitCode,
      stopRequested: stoppedByUser,
      output: log.toString(),
    );
    stopRequested = false;
    clearNetworkKey();
    notifyListeners();
    appendLog(
      stoppedByUser
          ? '\nTunD stopped.\n'
          : '\nTunD exited with code $exitCode.\n',
    );
  }

  void failWithGuidance(String title, String message) {
    if (disposed) return;
    status = GuiStatus.failed;
    stopRequested = false;
    guidedError = GuidedError(title: title, message: message);
    clearNetworkKey();
    notifyListeners();
  }

  void stopTund() {
    status = GuiStatus.stopping;
    stopRequested = true;
    guidedError = null;
    notifyListeners();
    appendLog('\nStopping TunD...\n');
    launcher.stop();
  }

  void generateKey() {
    key.text = generateNetworkKey();
    showKey = true;
    notifyListeners();
  }

  Future<void> copyKey({
    required Future<void> Function(String text) copyText,
    required void Function(String message) showError,
    required void Function(String message) showInfo,
  }) async {
    final value = key.text.trim();
    if (value.isEmpty) {
      showError('Enter or generate a network key first.');
      return;
    }

    await copyText(value);
    showInfo('Network key copied. Clear your clipboard after sharing it.');
  }

  Future<void> copyClientCommand({
    required Future<void> Function(String text) copyText,
    required void Function(String message) showInfo,
  }) async {
    await copyText(buildClientCommand(port: port.text));
    showInfo('Client command copied. Paste the network key when prompted.');
  }

  void acceptPrivilegeNotice() {
    privilegeNoticeAccepted = true;
    notifyListeners();
  }

  void setMode(TundMode value) {
    mode = value;
    notifyListeners();
  }

  void toggleKeyVisibility() {
    showKey = !showKey;
    notifyListeners();
  }

  void clearNetworkKey() {
    key.clear();
    showKey = false;
  }

  void setVerbose(bool value) {
    verbose = value;
    notifyListeners();
  }

  TundConfig currentConfig() {
    return TundConfig(
      mode: mode,
      server: server.text.trim(),
      port: port.text.trim(),
      name: name.text.trim(),
      key: key.text.trim(),
      verbose: verbose,
    );
  }

  void appendLog(String text) {
    if (disposed) return;
    log.write(text);
    notifyListeners();
  }

  void applyJsonEvent(String line) {
    if (disposed || mode != TundMode.server) return;
    try {
      if (hostPeerTracker.applyJsonLine(line)) {
        hostPeers = hostPeerTracker.peers;
      }
    } on FormatException {
      appendLog('$line\n');
      return;
    }
    notifyListeners();
  }
}

GuiStatus statusForTundExit(int exitCode, {required bool stopRequested}) {
  if (stopRequested || exitCode == 0) return GuiStatus.stopped;
  return GuiStatus.failed;
}

GuidedError? guidedErrorForTundExit(
  int exitCode, {
  required bool stopRequested,
  required String output,
}) {
  if (statusForTundExit(exitCode, stopRequested: stopRequested) !=
      GuiStatus.failed) {
    return null;
  }
  return guideTundFailure(output);
}
