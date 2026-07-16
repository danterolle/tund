import 'package:flutter/material.dart';

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
  const TundHeader({super.key, required this.status, required this.running});

  final String status;
  final bool running;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Container(
          width: 64,
          height: 64,
          decoration: BoxDecoration(
            color: TundColors.blue.withValues(alpha: 0.16),
            borderRadius: BorderRadius.circular(22),
            border: Border.all(color: TundColors.blue.withValues(alpha: 0.45)),
          ),
          child: const Center(
            child: Text('T',
                style: TextStyle(fontSize: 34, fontWeight: FontWeight.w900)),
          ),
        ),
        const SizedBox(width: 18),
        const Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Tund',
                  style: TextStyle(fontSize: 38, fontWeight: FontWeight.w900)),
              Text('Virtual LAN launcher for Windows',
                  style: TextStyle(color: TundColors.muted, fontSize: 15)),
            ],
          ),
        ),
        TundStatusPill(text: status, running: running),
      ],
    );
  }
}

class TundStatusPill extends StatelessWidget {
  const TundStatusPill({super.key, required this.text, required this.running});

  final String text;
  final bool running;

  @override
  Widget build(BuildContext context) {
    final color = running ? TundColors.green : TundColors.muted;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: color.withValues(alpha: 0.7)),
      ),
      child: Text(text,
          style: TextStyle(
              color: color, fontWeight: FontWeight.w800, fontSize: 12)),
    );
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
