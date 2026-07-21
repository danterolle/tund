import 'dart:io';

import 'package:flutter/material.dart';

import '../../core/error_guidance.dart';
import '../../core/host_peer.dart';
import '../../core/key_helpers.dart';
import '../../core/share_command.dart';
import '../../core/status.dart';
import '../../core/tund_config.dart';
import '../../core/tund_launcher.dart';

part 'key_actions.dart';
part 'lifecycle.dart';
part 'peer_events.dart';
part 'status_helpers.dart';

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
  }) {
    return _startTund(
      this,
      confirmPrivileges: confirmPrivileges,
      showError: showError,
    );
  }

  void prepareStart(TundConfig config) => _prepareStart(this, config);
  void markRunning() => _markRunning(this);
  void completeExit(int exitCode) => _completeExit(this, exitCode);
  void failWithGuidance(String title, String message) =>
      _failWithGuidance(this, title, message);
  void stopTund() => _stopTund(this);

  void generateKey() => _generateKey(this);

  Future<void> copyKey({
    required Future<void> Function(String text) copyText,
    required void Function(String message) showError,
    required void Function(String message) showInfo,
  }) {
    return _copyKey(
      this,
      copyText: copyText,
      showError: showError,
      showInfo: showInfo,
    );
  }

  Future<void> copyClientCommand({
    required Future<void> Function(String text) copyText,
    required void Function(String message) showInfo,
  }) {
    return _copyClientCommand(
      this,
      copyText: copyText,
      showInfo: showInfo,
    );
  }

  void acceptPrivilegeNotice() {
    privilegeNoticeAccepted = true;
    notifyListeners();
  }

  void setMode(TundMode value) {
    mode = value;
    notifyListeners();
  }

  void toggleKeyVisibility() => _toggleKeyVisibility(this);
  void clearNetworkKey() => _clearNetworkKey(this);

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

  void applyJsonEvent(String line) => _applyJsonEvent(this, line);
}
