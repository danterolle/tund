part of 'home_controller.dart';

GuiStatus statusForTundExit(int exitCode, {required bool stopRequested}) {
  if (stopRequested || exitCode == 0) return GuiStatus.stopped;
  return GuiStatus.failed;
}

GuidedError? guidedErrorForTundExit(
  int exitCode, {
  required bool stopRequested,
  required String output,
}) {
  if (statusForTundExit(exitCode, stopRequested: stopRequested) !=
      GuiStatus.failed) {
    return null;
  }
  return guideTundFailure(output);
}
