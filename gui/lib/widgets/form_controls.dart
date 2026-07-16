import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../theme.dart';
import '../tund_launcher.dart';

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
    return SegmentedButton<TundMode>(
      selected: {mode},
      onSelectionChanged: enabled ? (value) => onChanged(value.first) : null,
      segments: const [
        ButtonSegment(
            value: TundMode.server,
            icon: Icon(Icons.hub_outlined),
            label: Text('Host LAN')),
        ButtonSegment(
            value: TundMode.client,
            icon: Icon(Icons.link_outlined),
            label: Text('Join LAN')),
      ],
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
