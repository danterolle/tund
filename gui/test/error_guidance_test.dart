import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/error_guidance.dart';

void main() {
  test('explains missing Wintun', () {
    final error = guideTundFailure('Failed to load wintun.dll (error 126)');

    expect(error.title, 'Wintun is missing or unavailable');
  });

  test('explains missing privileges', () {
    final error = guideTundFailure(
      'Cannot open /dev/net/tun: Operation not permitted. Run with sudo/CAP_NET_ADMIN.',
    );

    expect(error.title, 'Missing privileges');
  });

  test('explains busy UDP ports', () {
    final error = guideTundFailure(
      'Cannot bind UDP port 9909: Address already in use.',
    );

    expect(error.title, 'UDP port is already in use');
  });

  test('explains unreachable servers', () {
    final error = guideTundFailure(
      'Registration timed out after 3 attempts; check server address.',
    );

    expect(error.title, 'Cannot reach the server');
  });

  test('falls back for unknown errors', () {
    final error = guideTundFailure('Unexpected process failure');

    expect(error.title, 'TunD stopped with an error');
  });
}
