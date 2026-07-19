import 'package:flutter/material.dart';

import 'home_page.dart';
import 'theme.dart';

class TundApp extends StatelessWidget {
  const TundApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'TunD',
      theme: TundTheme.data,
      home: const TundHomePage(),
    );
  }
}
