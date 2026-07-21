import 'dart:math';

import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/key_helpers.dart';
import 'package:tund_gui/core/share_command.dart';
import 'package:tund_gui/core/config.dart';

void main() {
  group('network key generation', () {
    test('generates valid keys with the requested length', () {
      final key = generateNetworkKey(length: 32, random: Random(1));

      expect(key, hasLength(32));
      expect(key, matches(RegExp(r'^[A-Za-z2-9]+$')));
    });

    test('rejects keys shorter than the CLI minimum', () {
      expect(() => generateNetworkKey(length: 11, random: Random(1)),
          throwsArgumentError);
    });
  });

  group('client share command', () {
    test('builds a client command that asks for the key at runtime', () {
      final command = buildClientCommand(
        port: '12345',
      );

      expect(command, 'tund-cli client -s SERVER_IP -p 12345 --key-stdin');
    });

    test('supports custom server values', () {
      final command = buildClientCommand(
        port: '9909',
        server: '203.0.113.10',
      );

      expect(command, 'tund-cli client -s 203.0.113.10 -p 9909 --key-stdin');
    });

    test('uses the default port for empty port values', () {
      final command = buildClientCommand(port: '');

      expect(command, 'tund-cli client -s SERVER_IP -p 9909 --key-stdin');
    });
  });

  group('TundConfig validation', () {
    test('accepts a valid server configuration', () {
      const config = TundConfig(
        mode: TundMode.server,
        server: '',
        port: '9909',
        name: '',
        key: 'a-long-random-key',
        verbose: false,
      );

      expect(config.validate(), isNull);
    });

    test('rejects invalid ports and short keys', () {
      const invalidPort = TundConfig(
        mode: TundMode.server,
        server: '',
        port: '0',
        name: '',
        key: 'a-long-random-key',
        verbose: false,
      );
      const shortKey = TundConfig(
        mode: TundMode.server,
        server: '',
        port: '9909',
        name: '',
        key: 'short',
        verbose: false,
      );

      expect(invalidPort.validate(), 'Use a port between 1 and 65535.');
      expect(
          shortKey.validate(), 'Use a network key of at least 12 characters.');
    });

    test('requires a server address in client mode', () {
      const config = TundConfig(
        mode: TundMode.client,
        server: '',
        port: '9909',
        name: 'Bridge',
        key: 'a-long-random-key',
        verbose: false,
      );

      expect(config.validate(), 'Enter the server IP or hostname.');
    });
  });

  group('TundConfig arguments', () {
    test('builds server arguments', () {
      const config = TundConfig(
        mode: TundMode.server,
        server: '',
        port: '9909',
        name: '',
        key: 'a-long-random-key',
        verbose: false,
      );

      expect(config.toArgs(), [
        'server',
        '-p',
        '9909',
        '--key-stdin',
        '-t',
        '--json-events',
      ]);
    });

    test('builds client arguments with default name and verbose flag', () {
      const config = TundConfig(
        mode: TundMode.client,
        server: '203.0.113.10',
        port: '12345',
        name: '',
        key: 'a-long-random-key',
        verbose: true,
      );

      expect(config.toArgs(), [
        'client',
        '-p',
        '12345',
        '--key-stdin',
        '-t',
        '-s',
        '203.0.113.10',
        '-n',
        'tund-client',
        '-v',
      ]);
    });

    test('hides the network key in display arguments', () {
      const config = TundConfig(
        mode: TundMode.client,
        server: 'example.test',
        port: '9909',
        name: 'Gaming PC',
        key: 'a-long-random-key',
        verbose: false,
      );

      expect(config.displayArgs(),
          'client -p 9909 --key-stdin -t -s example.test -n "Gaming PC"');
    });
  });
}
