import 'package:flutter/material.dart';

import '../theme.dart';

class TundActions extends StatelessWidget {
  const TundActions({
    super.key,
    required this.running,
    required this.onStart,
    required this.onStop,
  });

  final bool running;
  final VoidCallback onStart;
  final VoidCallback onStop;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Expanded(
          child: FilledButton.icon(
            onPressed: running ? null : onStart,
            icon: const Icon(Icons.play_arrow_rounded),
            label: const Text('Start'),
            style: FilledButton.styleFrom(
              minimumSize: const Size.fromHeight(52),
              backgroundColor: TundColors.blue,
              foregroundColor: TundColors.text,
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(16)),
            ),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: OutlinedButton.icon(
            onPressed: running ? onStop : null,
            icon: const Icon(Icons.stop_rounded),
            label: const Text('Stop'),
            style: OutlinedButton.styleFrom(
              minimumSize: const Size.fromHeight(52),
              foregroundColor: TundColors.text,
              side: const BorderSide(color: TundColors.border),
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(16)),
            ),
          ),
        ),
      ],
    );
  }
}
