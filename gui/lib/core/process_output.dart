import 'dart:convert';

const tundProcessOutputDecoder = Utf8Decoder(allowMalformed: true);

final _ansiEscapePattern = RegExp(
  '\u001B(?:'
  r'[@-Z\\-_]'
  r'|\[[0-?]*[ -/]*[@-~]'
  r'|\][^\u0007]*(?:\u0007|\u001B\\)'
  r')',
);

String sanitizeProcessOutput(String text) {
  return text.replaceAll(_ansiEscapePattern, '');
}
