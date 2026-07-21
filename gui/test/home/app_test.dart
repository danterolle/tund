import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/app.dart';

import '../support/test_helpers.dart';

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
    expect(find.text('Connected peers'), findsOneWidget);
    expect(find.text('Start hosting to see connected peers.'), findsOneWidget);
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
    expect(find.text('Connected peers'), findsNothing);
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
