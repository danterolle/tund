import 'package:flutter_test/flutter_test.dart';

import 'package:tund_gui/main.dart';

void main() {
  testWidgets('renders the launcher shell', (tester) async {
    await tester.pumpWidget(const TundApp());

    expect(find.text('Tund'), findsOneWidget);
    expect(find.text('Host LAN'), findsOneWidget);
    expect(find.text('Join LAN'), findsOneWidget);
  });
}
