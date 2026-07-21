import 'package:flutter/material.dart';
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
