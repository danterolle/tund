import 'package:flutter/material.dart';

class TundColors {
  static const bg = Color(0xFF14181B);
  static const bg2 = Color(0xFF101316);
  static const card = Color(0xFF1E2327);
  static const field = Color(0xFF262C31);
  static const border = Color(0xFF333A40);
  static const text = Color(0xFFF2F3F4);
  static const muted = Color(0xFFA8B0B6);
  static const faint = Color(0xFF6B7379);
  static const blue = Color(0xFF378ADD);
  static const blue2 = Color(0xFF85B7EB);
  static const green = Color(0xFF5CD39A);
  static const yellow = Color(0xFFE3B85C);
  static const red = Color(0xFFE16B6B);
}

class TundTheme {
  static ThemeData get data {
    return ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      fontFamily: 'Segoe UI',
      scaffoldBackgroundColor: TundColors.bg,
      colorScheme: const ColorScheme.dark(
        primary: TundColors.blue,
        surface: TundColors.card,
      ),
    );
  }
}
