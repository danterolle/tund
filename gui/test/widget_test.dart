import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:tund_gui/app.dart';

Finder fieldByLabel(String label) {
  final textField = find.ancestor(
    of: find.text(label),
    matching: find.byType(TextField),
  );
  return find.descendant(of: textField, matching: find.byType(EditableText));
}

Future<void> pumpAppAndAcceptPrivilegeNotice(WidgetTester tester) async {
  await tester.pumpWidget(const TundApp());
  await tester.pumpAndSettle();
  await tester.tap(find.text('I understand'));
  await tester.pumpAndSettle();
}

void main() {
  testWidgets('shows the privileges notice on startup', (tester) async {
    await tester.pumpWidget(const TundApp());
    await tester.pumpAndSettle();

    expect(find.text('Privileges required'), findsOneWidget);
    expect(find.text('I understand'), findsOneWidget);
  });

  testWidgets('renders the launcher shell', (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    expect(find.text('TunD'), findsOneWidget);
    expect(find.text('Ready'), findsWidgets);
    expect(find.text('Choose a mode, enter the shared key, then start TunD.'),
        findsOneWidget);
    expect(find.text('Host a LAN'), findsOneWidget);
    expect(find.text('Join a LAN'), findsOneWidget);
  });

  testWidgets('switches between host and join forms', (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    expect(
        find.text(
            'This computer becomes the hub at 10.9.0.1. Allow inbound UDP on the selected port.'),
        findsOneWidget);
    expect(find.text('Server IP or hostname'), findsNothing);

    await tester.tap(find.text('Join a LAN'));
    await tester.pumpAndSettle();

    expect(find.text('Server IP or hostname'), findsOneWidget);
    expect(
        find.text(
            'This computer becomes the hub at 10.9.0.1. Allow inbound UDP on the selected port.'),
        findsNothing);
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

  testWidgets('generates a network key from the launcher form',
      (tester) async {
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
    final messenger = TestDefaultBinaryMessengerBinding
        .instance.defaultBinaryMessenger;
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
    expect(find.text('Network key copied.'), findsOneWidget);
  });

  testWidgets('shows and copies the host client command', (tester) async {
    final messenger = TestDefaultBinaryMessengerBinding
        .instance.defaultBinaryMessenger;
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
      find.text('tund-cli client -s SERVER_IP -p 12345 -k "********"'),
      findsOneWidget,
    );

    await tester.tap(find.text('Copy client command'));
    await tester.pumpAndSettle();

    expect(clipboardPayload, {
      'text': 'tund-cli client -s SERVER_IP -p 12345 -k "a-long-random-key"',
    });
    expect(find.text('Client command copied.'), findsOneWidget);
  });

  testWidgets('does not repeat the privileges dialog after startup acceptance',
      (tester) async {
    await pumpAppAndAcceptPrivilegeNotice(tester);

    await tester.enterText(fieldByLabel('Network key'), 'a-long-random-key');
    await tester.ensureVisible(find.text('Start'));
    await tester.tap(find.text('Start'));
    await tester.pumpAndSettle();

    expect(find.text('Privileges required'), findsNothing);
  });
}
