import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'theme.dart';
import 'tund_launcher.dart';

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
