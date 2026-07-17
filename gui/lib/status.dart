enum GuiStatus {
  ready('Ready', 'Choose a mode, enter the shared key, then start Tund.'),
  starting('Starting', 'Launching tund-cli and waiting for process output.'),
  running(
      'Running', 'tund-cli is running. Keep this window open while connected.'),
  stopping('Stopping', 'Stopping tund-cli and closing the tunnel.'),
  stopped('Stopped', 'tund-cli exited cleanly. You can start it again.'),
  failed('Failed', 'tund-cli stopped with an error. Check the logs below.');

  const GuiStatus(this.label, this.description);

  final String label;
  final String description;
}
