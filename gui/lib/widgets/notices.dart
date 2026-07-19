import 'package:flutter/material.dart';

import '../error_guidance.dart';
import '../theme.dart';

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
