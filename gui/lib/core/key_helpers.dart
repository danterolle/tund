import 'dart:math';

const _keyAlphabet =
    'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789';

String generateNetworkKey({int length = 32, Random? random}) {
  if (length < 12) {
    throw ArgumentError.value(length, 'length', 'must be at least 12');
  }

  final source = random ?? Random.secure();
  return String.fromCharCodes(
    List.generate(
      length,
      (_) => _keyAlphabet.codeUnitAt(source.nextInt(_keyAlphabet.length)),
    ),
  );
}
