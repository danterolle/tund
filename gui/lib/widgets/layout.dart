import 'package:flutter/material.dart';

import '../error_guidance.dart';
import '../status.dart';
import '../theme.dart';

class TundShell extends StatelessWidget {
  const TundShell({super.key, required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: const BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [TundColors.bg, TundColors.bg2],
        ),
      ),
      child: Center(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(28),
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 820),
            child: TundPanel(child: child),
          ),
        ),
      ),
    );
  }
}

class TundPanel extends StatelessWidget {
  const TundPanel({super.key, required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(28),
      decoration: BoxDecoration(
        color: TundColors.card.withValues(alpha: 0.96),
        borderRadius: BorderRadius.circular(30),
        border: Border.all(color: TundColors.border),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.28),
            blurRadius: 36,
            offset: const Offset(0, 18),
          ),
        ],
      ),
      child: child,
    );
  }
}

class TundHeader extends StatelessWidget {
  const TundHeader({super.key, required this.status});

  final GuiStatus status;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        ClipRRect(
          borderRadius: BorderRadius.circular(18),
          child: Image.asset(
            'assets/logo.png',
            width: 64,
            height: 64,
            semanticLabel: 'Tund logo',
          ),
        ),
        const SizedBox(width: 18),
        const Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Tund',
                  style: TextStyle(fontSize: 38, fontWeight: FontWeight.w900)),
              Text('Virtual LAN launcher',
                  style: TextStyle(color: TundColors.muted, fontSize: 15)),
            ],
          ),
        ),
        TundStatusPill(status: status),
      ],
    );
  }
}

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
      return Icons.radio_button_unchecked;
  }
}

class TundHostNotice extends StatelessWidget {
  const TundHostNotice({super.key});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: TundColors.blue.withValues(alpha: 0.10),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.blue.withValues(alpha: 0.32)),
      ),
      child: const Row(
        children: [
          Icon(Icons.router_outlined, color: TundColors.blue2),
          SizedBox(width: 12),
          Expanded(
            child: Text(
              'This computer becomes the hub at 10.9.0.1. Allow inbound UDP on the selected port.',
              style: TextStyle(color: TundColors.muted, height: 1.35),
            ),
          ),
        ],
      ),
    );
  }
}

class TundPrivilegeNotice extends StatelessWidget {
  const TundPrivilegeNotice({super.key, required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: TundColors.field.withValues(alpha: 0.72),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.border),
      ),
      child: Row(
        children: [
          const Icon(Icons.admin_panel_settings_outlined,
              color: TundColors.blue2),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              message,
              style: const TextStyle(color: TundColors.muted, height: 1.35),
            ),
          ),
        ],
      ),
    );
  }
}

class TundGuidedErrorNotice extends StatelessWidget {
  const TundGuidedErrorNotice({super.key, required this.error});

  final GuidedError error;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: TundColors.red.withValues(alpha: 0.10),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.red.withValues(alpha: 0.36)),
      ),
      child: Row(
        children: [
          const Icon(Icons.tips_and_updates_outlined, color: TundColors.red),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  error.title,
                  style: const TextStyle(
                    color: TundColors.red,
                    fontWeight: FontWeight.w800,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  error.message,
                  style: const TextStyle(
                    color: TundColors.muted,
                    height: 1.35,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
