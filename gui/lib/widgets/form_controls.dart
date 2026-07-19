import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../theme.dart';
import '../core/tund_config.dart';

class TundModeSelector extends StatelessWidget {
  const TundModeSelector({
    super.key,
    required this.mode,
    required this.enabled,
    required this.onChanged,
  });

  final TundMode mode;
  final bool enabled;
  final ValueChanged<TundMode> onChanged;

  @override
  Widget build(BuildContext context) {
    final host = TundModeCard(
      title: 'Host a LAN',
      description: 'Create the virtual LAN and let friends join this computer.',
      icon: Icons.hub_outlined,
      selected: mode == TundMode.server,
      enabled: enabled,
      onTap: () => onChanged(TundMode.server),
    );
    final join = TundModeCard(
      title: 'Join a LAN',
      description:
          'Connect this computer to a TunD host that is already running.',
      icon: Icons.link_outlined,
      selected: mode == TundMode.client,
      enabled: enabled,
      onTap: () => onChanged(TundMode.client),
    );

    return LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth < 520) {
          return Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              host,
              const SizedBox(height: 14),
              join,
            ],
          );
        }
        return Row(
          children: [
            Expanded(child: host),
            const SizedBox(width: 14),
            Expanded(child: join),
          ],
        );
      },
    );
  }
}

class TundModeCard extends StatelessWidget {
  const TundModeCard({
    super.key,
    required this.title,
    required this.description,
    required this.icon,
    required this.selected,
    required this.enabled,
    required this.onTap,
  });

  final String title;
  final String description;
  final IconData icon;
  final bool selected;
  final bool enabled;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final borderColor = selected ? TundColors.blue2 : TundColors.border;
    return InkWell(
      onTap: enabled ? onTap : null,
      borderRadius: BorderRadius.circular(18),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 140),
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: selected
              ? TundColors.blue.withValues(alpha: 0.12)
              : TundColors.field.withValues(alpha: enabled ? 0.72 : 0.38),
          borderRadius: BorderRadius.circular(18),
          border: Border.all(color: borderColor, width: selected ? 1.4 : 1),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(icon,
                    color: selected ? TundColors.blue2 : TundColors.muted),
                const Spacer(),
                Icon(
                  selected
                      ? Icons.radio_button_checked
                      : Icons.radio_button_unchecked,
                  color: selected ? TundColors.blue2 : TundColors.faint,
                  size: 18,
                ),
              ],
            ),
            const SizedBox(height: 12),
            Text(
              title,
              style: const TextStyle(
                color: TundColors.text,
                fontWeight: FontWeight.w800,
                fontSize: 15,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              description,
              style: TextStyle(
                color: enabled ? TundColors.muted : TundColors.faint,
                fontSize: 12.5,
                height: 1.32,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class TundField extends StatelessWidget {
  const TundField({
    super.key,
    required this.controller,
    required this.label,
    required this.running,
    this.hint,
    this.enabled = true,
    this.obscure = false,
    this.maxLength,
    this.formatters,
    this.suffix,
  });

  final TextEditingController controller;
  final String label;
  final bool running;
  final String? hint;
  final bool enabled;
  final bool obscure;
  final int? maxLength;
  final List<TextInputFormatter>? formatters;
  final Widget? suffix;

  @override
  Widget build(BuildContext context) {
    final active = enabled && !running;
    return TextField(
      controller: controller,
      enabled: active,
      obscureText: obscure,
      maxLength: maxLength,
      inputFormatters: formatters,
      cursorColor: TundColors.blue2,
      decoration: InputDecoration(
        counterText: '',
        labelText: label,
        hintText: hint,
        suffixIcon: suffix,
        filled: true,
        fillColor: active
            ? TundColors.field
            : TundColors.field.withValues(alpha: 0.42),
        labelStyle: const TextStyle(color: TundColors.muted),
        hintStyle: const TextStyle(color: TundColors.faint),
        border: OutlineInputBorder(borderRadius: BorderRadius.circular(16)),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(16),
          borderSide: const BorderSide(color: TundColors.border),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(16),
          borderSide: const BorderSide(color: TundColors.blue2),
        ),
      ),
    );
  }
}
