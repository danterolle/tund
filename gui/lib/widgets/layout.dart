import 'package:flutter/material.dart';

import '../status.dart';
import '../theme.dart';
import 'status_widgets.dart';

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
      child: SafeArea(
        child: Center(
          child: Padding(
            padding: const EdgeInsets.all(28),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 1160),
              child: child,
            ),
          ),
        ),
      ),
    );
  }
}

class TundCard extends StatelessWidget {
  const TundCard({super.key, required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        color: TundColors.card.withValues(alpha: 0.96),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: TundColors.border),
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
            semanticLabel: 'TunD logo',
          ),
        ),
        const SizedBox(width: 18),
        const Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('TunD',
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
