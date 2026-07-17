import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'status.dart';
import 'theme.dart';
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

  bool get running => status == GuiStatus.starting || launcher.running;

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
      setState(
          () => status = exitCode == 0 ? GuiStatus.stopped : GuiStatus.failed);
      appendLog('\nTund exited with code $exitCode.\n');
    } on TundLaunchException catch (error) {
      if (!mounted) return;
      setState(() => status = GuiStatus.failed);
      showError(error.message);
    } catch (error) {
      if (!mounted) return;
      setState(() => status = GuiStatus.failed);
      showError('Cannot start tund-cli.exe: $error');
    }
  }

  void stopTund() {
    setState(() => status = GuiStatus.stopping);
    appendLog('\nStopping Tund...\n');
    launcher.stop();
  }

  Future<bool> confirmPrivileges() async {
    if (privilegeNoticeAccepted) return true;

    final accepted = await showDialog<bool>(
      context: context,
      builder: (context) {
        return AlertDialog(
          title: const Text('Privileges required'),
          content: Text(privilegeMessage()),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.of(context).pop(true),
              child: const Text('Continue'),
            ),
          ],
        );
      },
    );
    if (accepted == true && mounted) {
      setState(() => privilegeNoticeAccepted = true);
      return true;
    }
    return false;
  }

  String privilegeMessage() {
    if (Platform.isWindows) {
      return 'Tund needs Administrator privileges to create the virtual network adapter. Accept the Windows UAC prompt if it appears.';
    }
    return 'Tund needs administrator/root privileges to create and configure the TUN interface. Launch the GUI with the required privileges, or run tund-cli directly with sudo if startup fails.';
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
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: TundColors.red,
        behavior: SnackBarBehavior.floating,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: TundShell(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            TundHeader(status: status, running: running),
            const SizedBox(height: 16),
            TundStatusCard(status: status),
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
            Row(
              children: [
                SizedBox(
                  width: 150,
                  child: TundField(
                    controller: port,
                    label: 'UDP port',
                    running: running,
                    maxLength: 5,
                    formatters: [FilteringTextInputFormatter.digitsOnly],
                  ),
                ),
                const SizedBox(width: 14),
                Expanded(
                  child: TundField(
                    controller: name,
                    label: 'Display name',
                    running: running,
                    enabled: mode == TundMode.client,
                    maxLength: 31,
                  ),
                ),
              ],
            ),
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
            Row(
              children: [
                Switch(
                  value: verbose,
                  onChanged: running
                      ? null
                      : (value) => setState(() => verbose = value),
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
            const SizedBox(height: 18),
            TundLogBox(text: log.toString(), controller: logScroll),
          ],
        ),
      ),
    );
  }
}
