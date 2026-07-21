import 'package:flutter/material.dart';

import '../theme.dart';

class TundHostSharePanel extends StatelessWidget {
  const TundHostSharePanel({
    super.key,
    required this.command,
    required this.canCopy,
    required this.onCopy,
  });

  final String command;
  final bool canCopy;
  final VoidCallback onCopy;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: TundColors.blue.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.blue.withValues(alpha: 0.28)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Row(
            children: [
              Icon(Icons.ios_share_rounded, color: TundColors.blue2),
              SizedBox(width: 10),
              Expanded(
                child: Text(
                  'Share with clients',
                  style: TextStyle(
                    color: TundColors.text,
                    fontWeight: FontWeight.w800,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: TundColors.bg2,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: TundColors.border),
            ),
            child: SelectableText(
              command,
              style: const TextStyle(
                color: TundColors.muted,
                fontFamily: 'Cascadia Mono',
                fontFamilyFallback: ['Consolas', 'monospace'],
                fontSize: 12.5,
                height: 1.35,
              ),
            ),
          ),
          const SizedBox(height: 10),
          OutlinedButton.icon(
            onPressed: canCopy ? onCopy : null,
            icon: const Icon(Icons.copy_rounded),
            label: const Text('Copy client command'),
          ),
          const SizedBox(height: 8),
          const Text(
            'Replace SERVER_IP with this host\'s reachable LAN or public IP. Clients paste or type the network key when the command asks for it.',
            style: TextStyle(color: TundColors.faint, fontSize: 12),
          ),
        ],
      ),
    );
  }
}
