import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/process_output.dart';
import 'package:tund_gui/core/launcher/launcher.dart';

void main() {
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
