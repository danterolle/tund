import 'package:flutter/material.dart';

import '../theme.dart';

class TundLogBox extends StatelessWidget {
  const TundLogBox({super.key, required this.text, required this.controller});

  final String text;
  final ScrollController controller;

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 180,
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: TundColors.bg2,
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.border),
      ),
      child: Scrollbar(
        controller: controller,
        thumbVisibility: true,
        child: SingleChildScrollView(
          controller: controller,
          child: SelectableText(
            text.isEmpty ? 'Logs will appear here after Tund starts.' : text,
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
    );
  }
}
