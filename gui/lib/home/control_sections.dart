part of 'controls.dart';

extension HomeControlSections on HomeControls {
  Widget hostSharePanel() {
    return ValueListenableBuilder<TextEditingValue>(
      valueListenable: port,
      builder: (context, portValue, _) {
        return TundHostSharePanel(
          command: buildClientCommand(port: portValue.text),
          canCopy: true,
          onCopy: () {
            onCopyClientCommand();
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
