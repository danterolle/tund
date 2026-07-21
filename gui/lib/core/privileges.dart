import 'dart:io';

import 'package:flutter/material.dart';

String privilegeMessage() {
  if (Platform.isWindows) {
    return 'TunD needs Administrator privileges to create the virtual network adapter. The Windows GUI relaunches through UAC before it opens so the network key can still be sent safely through stdin.';
  }
  if (Platform.isMacOS) {
    return 'TunD needs administrator privileges to create and configure the TUN interface. On macOS release builds, use tund-gui.command next to tund-gui.app.';
  }
  return 'TunD needs administrator/root privileges to create and configure the TUN interface. Launch the GUI with the required privileges, or run tund-cli directly with sudo if startup fails.';
}

Future<bool?> showPrivilegeDialog(
  BuildContext context, {
  required String primaryLabel,
  required bool showCancel,
  required bool barrierDismissible,
}) {
  return showDialog<bool>(
    context: context,
    barrierDismissible: barrierDismissible,
    builder: (context) {
      return AlertDialog(
        title: const Text('Privileges required'),
        content: Text(privilegeMessage()),
        actions: [
          if (showCancel)
            TextButton(
              onPressed: () => Navigator.of(context).pop(false),
              child: const Text('Cancel'),
            ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: Text(primaryLabel),
          ),
        ],
      );
    },
  );
}
