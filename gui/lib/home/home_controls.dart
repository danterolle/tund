import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/error_guidance.dart';
import '../core/share_command.dart';
import '../core/status.dart';
import '../theme.dart';
import '../core/tund_config.dart';
import '../widgets/widgets.dart';

class HomeControls extends StatelessWidget {
  const HomeControls({
    super.key,
    required this.status,
    required this.guidedError,
    required this.privilegeMessage,
    required this.mode,
    required this.running,
    required this.showKey,
    required this.verbose,
    required this.server,
    required this.port,
    required this.name,
    required this.networkKey,
    required this.onModeChanged,
    required this.onToggleKeyVisibility,
    required this.onGenerateKey,
    required this.onCopyKey,
    required this.onCopyClientCommand,
    required this.onVerboseChanged,
    required this.onStart,
    required this.onStop,
  });

  final GuiStatus status;
  final GuidedError? guidedError;
  final String privilegeMessage;
  final TundMode mode;
  final bool running;
  final bool showKey;
  final bool verbose;
  final TextEditingController server;
  final TextEditingController port;
  final TextEditingController name;
  final TextEditingController networkKey;
  final ValueChanged<TundMode> onModeChanged;
  final VoidCallback onToggleKeyVisibility;
  final VoidCallback onGenerateKey;
  final Future<void> Function() onCopyKey;
  final Future<void> Function() onCopyClientCommand;
  final ValueChanged<bool> onVerboseChanged;
  final Future<void> Function() onStart;
  final VoidCallback onStop;

  @override
  Widget build(BuildContext context) {
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
        TundPrivilegeNotice(message: privilegeMessage),
        const SizedBox(height: 20),
        TundModeSelector(
          mode: mode,
          enabled: !running,
          onChanged: onModeChanged,
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
          controller: networkKey,
          label: 'Network key',
          hint: 'at least 12 characters',
          running: running,
          obscure: !showKey,
          maxLength: 127,
          suffix: IconButton(
            onPressed: onToggleKeyVisibility,
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
              onChanged: running ? null : onVerboseChanged,
            ),
            const Text('Verbose logs',
                style: TextStyle(color: TundColors.muted)),
            const Spacer(),
            const Text('Authenticated, not encrypted',
                style: TextStyle(color: TundColors.faint, fontSize: 12)),
          ],
        ),
        const SizedBox(height: 16),
        TundActions(
          running: running,
          onStart: () {
            onStart();
          },
          onStop: onStop,
        ),
      ],
    );
  }

  Widget hostSharePanel() {
    return ValueListenableBuilder<TextEditingValue>(
      valueListenable: networkKey,
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
              onCopy: () {
                onCopyClientCommand();
              },
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
              onPressed: running ? null : onGenerateKey,
              icon: const Icon(Icons.auto_fix_high_rounded),
              label: const Text('Generate key'),
            );
            final copyButton = OutlinedButton.icon(
              onPressed: running
                  ? null
                  : () {
                      onCopyKey();
                    },
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
