import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'error_guidance.dart';
import 'key_helpers.dart';
import 'privileges.dart';
import 'share_command.dart';
import 'status.dart';
import 'theme.dart';
import 'tund_config.dart';
import 'tund_launcher.dart';
import 'widgets/widgets.dart';

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

  TundMode mode = TundMode.server;
  bool verbose = false;
  bool showKey = false;
  bool privilegeNoticeAccepted = false;
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
      setState(() {
        status = exitCode == 0 ? GuiStatus.stopped : GuiStatus.failed;
        guidedError = exitCode == 0 ? null : guideTundFailure(log.toString());
      });
      appendLog('\nTunD exited with code $exitCode.\n');
    } on TundLaunchException catch (error) {
      if (!mounted) return;
      setState(() {
        status = GuiStatus.failed;
        guidedError = GuidedError(
          title: 'tund-cli was not found',
          message: error.message,
        );
      });
      showError(error.message);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        status = GuiStatus.failed;
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
    setState(() => status = GuiStatus.stopping);
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
    setState(() => log.write(text));
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
                    child: TundLogBox(
                      text: log.toString(),
                      controller: logScroll,
                      expanded: true,
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
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        TundHeader(status: status),
        const SizedBox(height: 16),
        TundStatusCard(status: status),
        if (guidedError != null) ...[
          const SizedBox(height: 12),
          TundGuidedErrorNotice(error: guidedError!),
        ],
        const SizedBox(height: 20),
        TundPrivilegeNotice(message: privilegeMessage()),
        const SizedBox(height: 20),
        TundModeSelector(
          mode: mode,
          enabled: !running,
          onChanged: (value) => setState(() => mode = value),
        ),
        const SizedBox(height: 20),
        if (mode == TundMode.client)
          TundField(
            controller: server,
            label: 'Server IP or hostname',
            hint: '203.0.113.10',
            running: running,
            maxLength: 63,
          )
        else
          const TundHostNotice(),
        const SizedBox(height: 16),
        endpointFields(),
        const SizedBox(height: 16),
        TundField(
          controller: key,
          label: 'Network key',
          hint: 'at least 12 characters',
          running: running,
          obscure: !showKey,
          maxLength: 127,
          suffix: IconButton(
            onPressed: () => setState(() => showKey = !showKey),
            icon: Icon(showKey
                ? Icons.visibility_off_outlined
                : Icons.visibility_outlined),
          ),
        ),
        const SizedBox(height: 10),
        keyHelpers(),
        if (mode == TundMode.server) ...[
          const SizedBox(height: 16),
          hostSharePanel(),
        ],
        const SizedBox(height: 10),
        Row(
          children: [
            Switch(
              value: verbose,
              onChanged:
                  running ? null : (value) => setState(() => verbose = value),
            ),
            const Text('Verbose logs',
                style: TextStyle(color: TundColors.muted)),
            const Spacer(),
            const Text('Authenticated, not encrypted',
                style: TextStyle(color: TundColors.faint, fontSize: 12)),
          ],
        ),
        const SizedBox(height: 16),
        TundActions(running: running, onStart: startTund, onStop: stopTund),
      ],
    );
  }

  Widget hostSharePanel() {
    return ValueListenableBuilder<TextEditingValue>(
      valueListenable: key,
      builder: (context, keyValue, _) {
        return ValueListenableBuilder<TextEditingValue>(
          valueListenable: port,
          builder: (context, portValue, _) {
            final hasKey = keyValue.text.trim().isNotEmpty;
            return TundHostSharePanel(
              command: buildClientCommand(
                port: portValue.text,
                key: keyValue.text,
                maskKey: hasKey,
              ),
              canCopy: !running && hasKey,
              onCopy: copyClientCommand,
            );
          },
        );
      },
    );
  }

  Widget keyHelpers() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        LayoutBuilder(
          builder: (context, constraints) {
            final generateButton = OutlinedButton.icon(
              onPressed: running ? null : generateKey,
              icon: const Icon(Icons.auto_fix_high_rounded),
              label: const Text('Generate key'),
            );
            final copyButton = OutlinedButton.icon(
              onPressed: running ? null : copyKey,
              icon: const Icon(Icons.copy_rounded),
              label: const Text('Copy key'),
            );

            if (constraints.maxWidth < 430) {
              return Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  generateButton,
                  const SizedBox(height: 10),
                  copyButton,
                ],
              );
            }
            return Row(
              children: [
                Expanded(child: generateButton),
                const SizedBox(width: 12),
                Expanded(child: copyButton),
              ],
            );
          },
        ),
        const SizedBox(height: 8),
        const Text(
          'Use the same key on every computer in this TunD LAN.',
          style: TextStyle(color: TundColors.faint, fontSize: 12),
        ),
      ],
    );
  }

  Widget endpointFields() {
    final portField = TundField(
      controller: port,
      label: 'UDP port',
      running: running,
      maxLength: 5,
      formatters: [FilteringTextInputFormatter.digitsOnly],
    );
    final nameField = TundField(
      controller: name,
      label: 'Display name',
      running: running,
      enabled: mode == TundMode.client,
      maxLength: 31,
    );

    return LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth < 430) {
          return Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              portField,
              const SizedBox(height: 14),
              nameField,
            ],
          );
        }
        return Row(
          children: [
            SizedBox(width: 150, child: portField),
            const SizedBox(width: 14),
            Expanded(child: nameField),
          ],
        );
      },
    );
  }
}
