import 'package:flutter/material.dart';

import '../theme.dart';

class TundLogBox extends StatelessWidget {
  const TundLogBox({
    super.key,
    required this.text,
    required this.controller,
    this.expanded = false,
  });

  final String text;
  final ScrollController controller;
  final bool expanded;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: TundColors.field.withValues(alpha: 0.42),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Row(
            children: [
              Icon(Icons.terminal_rounded, size: 18, color: TundColors.blue2),
              SizedBox(width: 8),
              Text(
                'Logs',
                style: TextStyle(
                  color: TundColors.text,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          SizedBox(
            height: expanded ? null : 148,
            child: Container(
              padding: const EdgeInsets.all(14),
              decoration: BoxDecoration(
                color: TundColors.bg2,
                borderRadius: BorderRadius.circular(14),
                border: Border.all(color: TundColors.border),
              ),
              child: Scrollbar(
                controller: controller,
                thumbVisibility: true,
                child: SingleChildScrollView(
                  padding: const EdgeInsets.all(14),
                  controller: controller,
                  child: SelectableText(
                    text.isEmpty
                        ? 'Logs will appear here after TunD starts.'
                        : text,
                    style: const TextStyle(
                      color: TundColors.muted,
                      fontFamily: 'Cascadia Mono',
                      fontFamilyFallback: ['Consolas', 'monospace'],
                      fontSize: 12.5,
                      height: 1.35,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
