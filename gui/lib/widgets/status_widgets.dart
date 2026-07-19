import 'package:flutter/material.dart';

import '../core/status.dart';
import '../theme.dart';

class TundStatusPill extends StatelessWidget {
  const TundStatusPill({super.key, required this.status});

  final GuiStatus status;

  @override
  Widget build(BuildContext context) {
    final color = statusColor(status);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: color.withValues(alpha: 0.7)),
      ),
      child: Text(status.label,
          style: TextStyle(
              color: color, fontWeight: FontWeight.w800, fontSize: 12)),
    );
  }
}

class TundStatusCard extends StatelessWidget {
  const TundStatusCard({super.key, required this.status});

  final GuiStatus status;

  @override
  Widget build(BuildContext context) {
    final color = statusColor(status);
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.10),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: color.withValues(alpha: 0.34)),
      ),
      child: Row(
        children: [
          Icon(statusIcon(status), color: color),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(status.label,
                    style:
                        TextStyle(color: color, fontWeight: FontWeight.w800)),
                const SizedBox(height: 2),
                Text(status.description,
                    style:
                        const TextStyle(color: TundColors.muted, height: 1.35)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

Color statusColor(GuiStatus status) {
  switch (status) {
    case GuiStatus.running:
    case GuiStatus.stopped:
      return TundColors.green;
    case GuiStatus.starting:
    case GuiStatus.stopping:
      return TundColors.yellow;
    case GuiStatus.failed:
      return TundColors.red;
    case GuiStatus.ready:
      return TundColors.muted;
  }
}

IconData statusIcon(GuiStatus status) {
  switch (status) {
    case GuiStatus.running:
      return Icons.play_circle_outline;
    case GuiStatus.starting:
      return Icons.sync_rounded;
    case GuiStatus.stopping:
      return Icons.stop_circle_outlined;
    case GuiStatus.stopped:
      return Icons.check_circle_outline;
    case GuiStatus.failed:
      return Icons.error_outline;
    case GuiStatus.ready:
      return Icons.tune_rounded;
  }
}
