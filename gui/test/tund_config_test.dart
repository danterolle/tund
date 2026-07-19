import 'dart:io';
import 'dart:math';

import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/key_helpers.dart';
import 'package:tund_gui/process_output.dart';
import 'package:tund_gui/share_command.dart';
import 'package:tund_gui/tund_config.dart';
import 'package:tund_gui/tund_launcher.dart';

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
    test('builds a client command with a quoted key', () {
      final command = buildClientCommand(
        port: '12345',
        key: 'key with spaces',
      );

      expect(command,
          'tund-cli client -s SERVER_IP -p 12345 -k "key with spaces"');
    });

    test('masks the key for display', () {
      final command = buildClientCommand(
        port: '9909',
        key: 'a-long-random-key',
        maskKey: true,
      );

      expect(command, 'tund-cli client -s SERVER_IP -p 9909 -k "********"');
    });

    test('uses defaults for empty port and key placeholders', () {
      final command = buildClientCommand(port: '', key: '');

      expect(command, 'tund-cli client -s SERVER_IP -p 9909 -k "<network-key>"');
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

      expect(config.toArgs(),
          ['server', '-p', '9909', '-k', 'a-long-random-key', '-t']);
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
        '-k',
        'a-long-random-key',
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
          'client -p 9909 -k ******** -t -s example.test -n "Gaming PC"');
    });
  });

  group('TundLauncher platform messages', () {
    test('uses platform-specific executable names', () {
      final launcher = TundLauncher();
      final cliName = Platform.isWindows ? 'tund-cli.exe' : 'tund-cli';
      final guiName = Platform.isWindows ? 'tund-gui.exe' : 'tund-gui';
      final message = Platform.isMacOS
          ? 'Place $cliName next to $guiName.app or inside $guiName.app/Contents/MacOS.'
          : 'Place $cliName in the same folder as $guiName.';

      expect(launcher.primaryExecutableName, cliName);
      expect(launcher.guiExecutableName, guiName);
      expect(launcher.missingExecutableMessage, message);
    });
  });

  group('process output sanitization', () {
    test('removes ANSI color sequences', () {
      final output = sanitizeProcessOutput(
        '\u001B[33mWarning: TunD requires root privileges\u001B[0m\n',
      );

      expect(output, 'Warning: TunD requires root privileges\n');
    });

    test('removes terminal control sequences', () {
      final output = sanitizeProcessOutput(
        '\u001B[?25lStarting TunD\u001B[?25h\n',
      );

      expect(output, 'Starting TunD\n');
    });
  });
}
