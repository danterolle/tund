part of 'launcher.dart';

class TundLaunchException implements Exception {
  const TundLaunchException({
    required this.title,
    required this.message,
  });

  final String title;
  final String message;

  @override
  String toString() => message;
}
