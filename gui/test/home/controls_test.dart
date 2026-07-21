import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/status.dart';
import 'package:tund_gui/core/config.dart';
import 'package:tund_gui/home/controls.dart';
import 'package:tund_gui/widgets/log_box.dart';

import '../support/test_helpers.dart';

void main() {
  testWidgets('allows editing the display name in host mode', (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.enterText(fieldByLabel('Display name'), 'Host PC');
    await tester.pumpAndSettle();

    final field = tester.widget<EditableText>(fieldByLabel('Display name'));
    expect(field.controller.text, 'Host PC');
  });

  testWidgets('shows validation errors before launching', (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.enterText(fieldByLabel('UDP port'), '0');
    await tester.enterText(fieldByLabel('Network key'), 'a-long-random-key');
    await tester.ensureVisible(find.text('Start'));
    await tester.tap(find.text('Start'));
    await tester.pumpAndSettle();

    expect(find.text('Use a port between 1 and 65535.'), findsOneWidget);
  });

  testWidgets('requires a server address in join mode', (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.tap(find.text('Join a LAN'));
    await tester.pumpAndSettle();
    await tester.enterText(fieldByLabel('Network key'), 'a-long-random-key');
    await tester.ensureVisible(find.text('Start'));
    await tester.tap(find.text('Start'));
    await tester.pumpAndSettle();

    expect(find.text('Enter the server IP or hostname.'), findsOneWidget);
  });

  testWidgets('generates a network key from the launcher form', (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.ensureVisible(find.text('Generate key'));
    await tester.tap(find.text('Generate key'));
    await tester.pumpAndSettle();

    final field = tester.widget<EditableText>(fieldByLabel('Network key'));
    expect(field.controller.text, hasLength(32));
    expect(field.controller.text, matches(RegExp(r'^[A-Za-z2-9]+$')));
    expect(find.text('Use the same key on every computer in this TunD LAN.'),
        findsOneWidget);
  });

  testWidgets('copies the network key from the launcher form', (tester) async {
    final messenger =
        TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
    Object? clipboardPayload;
    messenger.setMockMethodCallHandler(SystemChannels.platform, (call) async {
      if (call.method == 'Clipboard.setData') {
        clipboardPayload = call.arguments;
      }
      return null;
    });
    addTearDown(() {
      messenger.setMockMethodCallHandler(SystemChannels.platform, null);
    });

    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.enterText(fieldByLabel('Network key'), 'a-long-random-key');
    await tester.ensureVisible(find.text('Copy key'));
    await tester.tap(find.text('Copy key'));
    await tester.pumpAndSettle();

    expect(clipboardPayload, {'text': 'a-long-random-key'});
    expect(
      find.text('Network key copied. Clear your clipboard after sharing it.'),
      findsOneWidget,
    );
  });

  testWidgets('shows and copies the host client command', (tester) async {
    final messenger =
        TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
    Object? clipboardPayload;
    messenger.setMockMethodCallHandler(SystemChannels.platform, (call) async {
      if (call.method == 'Clipboard.setData') {
        clipboardPayload = call.arguments;
      }
      return null;
    });
    addTearDown(() {
      messenger.setMockMethodCallHandler(SystemChannels.platform, null);
    });

    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.enterText(fieldByLabel('UDP port'), '12345');
    await tester.enterText(fieldByLabel('Network key'), 'a-long-random-key');
    await tester.ensureVisible(find.text('Copy client command'));
    await tester.pumpAndSettle();

    expect(find.text('Share with clients'), findsOneWidget);
    expect(
      find.text('tund-cli client -s SERVER_IP -p 12345 --key-stdin'),
      findsOneWidget,
    );

    await tester.tap(find.text('Copy client command'));
    await tester.pumpAndSettle();

    expect(clipboardPayload, {
      'text': 'tund-cli client -s SERVER_IP -p 12345 --key-stdin',
    });
    expect(
        find.text(
            'Client command copied. Paste the network key when prompted.'),
        findsOneWidget);
  });

  testWidgets('can copy the host client command while running', (tester) async {
    final port = TextEditingController(text: '12345');
    final server = TextEditingController();
    final name = TextEditingController();
    final key = TextEditingController(text: 'a-long-random-key');
    var copied = false;
    addTearDown(() {
      server.dispose();
      port.dispose();
      name.dispose();
      key.dispose();
    });

    await tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: SingleChildScrollView(
          child: HomeControls(
            status: GuiStatus.running,
            guidedError: null,
            privilegeMessage: 'Privileges are required.',
            mode: TundMode.server,
            running: true,
            showKey: false,
            verbose: false,
            server: server,
            port: port,
            name: name,
            networkKey: key,
            onModeChanged: (_) {},
            onToggleKeyVisibility: () {},
            onGenerateKey: () {},
            onCopyKey: () async {},
            onCopyClientCommand: () async {
              copied = true;
            },
            onVerboseChanged: (_) {},
            onStart: () async {},
            onStop: () {},
          ),
        ),
      ),
    ));

    await tester.ensureVisible(find.text('Copy client command'));
    await tester.tap(find.text('Copy client command'));
    await tester.pumpAndSettle();

    expect(copied, isTrue);
  });

  testWidgets('expanded logs keep a bounded vertical scroll area',
      (tester) async {
    final controller = ScrollController();
    addTearDown(controller.dispose);
    final text = List.generate(80, (i) => 'log line $i').join('\n');

    await tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: SizedBox(
          width: 640,
          height: 320,
          child: TundLogBox(
            text: text,
            controller: controller,
            expanded: true,
          ),
        ),
      ),
    ));

    expect(controller.position.maxScrollExtent, greaterThan(0));
  });
}
