import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/error_guidance.dart';
import '../core/host_peer.dart';
import '../core/key_helpers.dart';
import '../core/privileges.dart';
import '../core/share_command.dart';
import '../core/status.dart';
import '../theme.dart';
import '../core/tund_config.dart';
import '../core/tund_launcher.dart';
import '../widgets/widgets.dart';
import 'home_controls.dart';

class TundHomePage extends StatefulWidget {
  const TundHomePage({super.key});

  @override
  State<TundHomePage> createState() => _TundHomePageState();
}

class _TundHomePageState extends State<TundHomePage> {
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

  bool get running => status == GuiStatus.starting || launcher.running;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      showInitialPrivilegeNotice();
    });
  }

  @override
  void dispose() {
    launcher.dispose();
    server.dispose();
    port.dispose();
    name.dispose();
    key.dispose();
    logScroll.dispose();
    super.dispose();
  }

  Future<void> startTund() async {
    if (running) return;

    final config = currentConfig();
    final error = config.validate();
    if (error != null) {
      showError(error);
      return;
    }
    if (!await confirmPrivileges()) {
      return;
    }

    setState(() {
      status = GuiStatus.starting;
      guidedError = null;
      stopRequested = false;
      hostPeerTracker.clear();
      hostPeers = const [];
      log.clear();
    });
    appendLog(
        '> ${launcher.executable().uri.pathSegments.last} ${config.displayArgs()}\n\n');

    try {
      final exitCode = await launcher.start(
        config,
        appendLog,
        onStarted: () {
          if (mounted) {
            setState(() => status = GuiStatus.running);
          }
        },
      );
      if (!mounted) return;
      final stoppedByUser = stopRequested;
      setState(() {
        status = statusForTundExit(exitCode, stopRequested: stoppedByUser);
        guidedError = guidedErrorForTundExit(
          exitCode,
          stopRequested: stoppedByUser,
          output: log.toString(),
        );
        stopRequested = false;
      });
      appendLog(stoppedByUser
          ? '\nTunD stopped.\n'
          : '\nTunD exited with code $exitCode.\n');
    } on TundLaunchException catch (error) {
      if (!mounted) return;
      setState(() {
        status = GuiStatus.failed;
        stopRequested = false;
        guidedError = GuidedError(
          title: error.title,
          message: error.message,
        );
      });
      showError(error.message);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        status = GuiStatus.failed;
        stopRequested = false;
        guidedError = const GuidedError(
          title: 'Could not start tund-cli',
          message:
              'The process could not be launched. Check the app folder and raw logs.',
        );
      });
      showError('Cannot start ${launcher.primaryExecutableName}: $error');
    }
  }

  void stopTund() {
    setState(() {
      status = GuiStatus.stopping;
      stopRequested = true;
      guidedError = null;
    });
    appendLog('\nStopping TunD...\n');
    launcher.stop();
  }

  void generateKey() {
    setState(() {
      key.text = generateNetworkKey();
      showKey = true;
    });
  }

  Future<void> copyKey() async {
    final value = key.text.trim();
    if (value.isEmpty) {
      showError('Enter or generate a network key first.');
      return;
    }

    await Clipboard.setData(ClipboardData(text: value));
    showInfo('Network key copied.');
  }

  Future<void> copyClientCommand() async {
    final value = key.text.trim();
    if (value.isEmpty) {
      showError('Enter or generate a network key first.');
      return;
    }

    await Clipboard.setData(ClipboardData(
      text: buildClientCommand(port: port.text, key: value),
    ));
    showInfo('Client command copied.');
  }

  Future<void> showInitialPrivilegeNotice() async {
    if (privilegeNoticeAccepted || !mounted) return;

    final accepted = await showPrivilegeDialog(
      context,
      primaryLabel: 'I understand',
      showCancel: false,
      barrierDismissible: false,
    );
    if (accepted == true && mounted) {
      setState(() => privilegeNoticeAccepted = true);
    }
  }

  Future<bool> confirmPrivileges() async {
    if (privilegeNoticeAccepted) return true;

    final accepted = await showPrivilegeDialog(
      context,
      primaryLabel: 'Continue',
      showCancel: true,
      barrierDismissible: true,
    );
    if (accepted == true && mounted) {
      setState(() => privilegeNoticeAccepted = true);
      return true;
    }
    return false;
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
    if (!mounted) return;
    setState(() {
      log.write(text);
      if (mode == TundMode.server) {
        hostPeerTracker.applyLog(text);
        hostPeers = hostPeerTracker.peers;
      }
    });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (logScroll.hasClients) {
        logScroll.jumpTo(logScroll.position.maxScrollExtent);
      }
    });
  }

  void showError(String message) {
    showSnackBar(message, TundColors.red);
  }

  void showInfo(String message) {
    showSnackBar(message, TundColors.blue);
  }

  void showSnackBar(String message, Color backgroundColor) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: backgroundColor,
        behavior: SnackBarBehavior.floating,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: TundShell(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final wide = constraints.maxWidth >= 980;
            if (wide) {
              return Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(
                    flex: 6,
                    child: TundCard(
                      child: SingleChildScrollView(child: controls()),
                    ),
                  ),
                  const SizedBox(width: 20),
                  Expanded(
                    flex: 5,
                    child: Column(
                      children: [
                        if (mode == TundMode.server) ...[
                          TundPeerTable(peers: hostPeers, running: running),
                          const SizedBox(height: 18),
                        ],
                        Expanded(
                          child: TundLogBox(
                            text: log.toString(),
                            controller: logScroll,
                            expanded: true,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              );
            }
            return SingleChildScrollView(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  TundCard(child: controls()),
                  const SizedBox(height: 18),
                  if (mode == TundMode.server) ...[
                    TundPeerTable(peers: hostPeers, running: running),
                    const SizedBox(height: 18),
                  ],
                  TundLogBox(text: log.toString(), controller: logScroll),
                ],
              ),
            );
          },
        ),
      ),
    );
  }

  Widget controls() {
    return HomeControls(
      status: status,
      guidedError: guidedError,
      privilegeMessage: privilegeMessage(),
      mode: mode,
      running: running,
      showKey: showKey,
      verbose: verbose,
      server: server,
      port: port,
      name: name,
      networkKey: key,
      onModeChanged: (value) => setState(() => mode = value),
      onToggleKeyVisibility: () => setState(() => showKey = !showKey),
      onGenerateKey: generateKey,
      onCopyKey: copyKey,
      onCopyClientCommand: copyClientCommand,
      onVerboseChanged: (value) {
        setState(() => verbose = value);
      },
      onStart: startTund,
      onStop: stopTund,
    );
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
