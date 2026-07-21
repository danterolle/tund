import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/status.dart';
import 'package:tund_gui/home/controller/home_controller.dart';

void main() {
  test('treats a requested stop as a clean exit', () {
    expect(statusForTundExit(1, stopRequested: true), GuiStatus.stopped);
    expect(
      guidedErrorForTundExit(
        1,
        stopRequested: true,
        output: 'Failed to load wintun.dll',
      ),
      isNull,
    );
  });

  test('keeps unexpected non-zero exits as failures', () {
    final error = guidedErrorForTundExit(
      1,
      stopRequested: false,
      output: 'Failed to load wintun.dll',
    );

    expect(statusForTundExit(1, stopRequested: false), GuiStatus.failed);
    expect(error?.title, 'Wintun is missing or unavailable');
  });

  test('clears the network key after TunD exits', () {
    final controller = TundHomeController();
    addTearDown(controller.dispose);
    controller.key.text = 'a-long-random-key';
    controller.showKey = true;

    controller.completeExit(0);

    expect(controller.key.text, isEmpty);
    expect(controller.showKey, isFalse);
  });
}
