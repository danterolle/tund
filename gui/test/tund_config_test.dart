import 'dart:io';
import 'dart:math';

import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/key_helpers.dart';
import 'package:tund_gui/core/process_output.dart';
import 'package:tund_gui/core/share_command.dart';
import 'package:tund_gui/core/tund_config.dart';
import 'package:tund_gui/core/tund_launcher.dart';

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

      expect(
          command, 'tund-cli client -s SERVER_IP -p 9909 -k "<network-key>"');
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

      expect(config.toArgs(), ['server', '-p', '9909', '--key-stdin', '-t']);
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

    test('formats macOS privilege guidance', () {
      final message = macPrivilegeMessage(
        guiExecutablePath: "/tmp/TunD's.app/Contents/MacOS/tund-gui",
        cliExecutablePath: "/tmp/TunD's.app/Contents/MacOS/tund-cli",
        commandPath: "/tmp/TunD's.command",
      );

      expect(message, contains('launcher next to tund-gui.app'));
      expect(message, contains("'/tmp/TunD'\"'\"'s.command'"));
      expect(message, contains('Do not run sudo tund-gui.app'));
    });

    test('finds the macOS launcher next to the app bundle', () {
      expect(
        macGuiCommandPath(
          "/tmp/TunD's/tund-gui.app/Contents/MacOS/tund-gui",
        ),
        "/tmp/TunD's/tund-gui.command",
      );
      expect(macGuiCommandPath('/tmp/tund-gui'), '');
    });
  });

  group('process output sanitization', () {
    test('decodes UTF-8 CLI output on every platform', () {
      final bytes = [
        0xE2, 0x95, 0x94, // ╔
        0xE2, 0x98, 0x85, // ★
        0xE2, 0x86, 0x92, // →
      ];

      expect(tundProcessOutputDecoder.convert(bytes), '╔★→');
    });

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
