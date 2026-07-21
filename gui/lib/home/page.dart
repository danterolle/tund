import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/privileges.dart';
import '../theme.dart';
import 'controller/home_controller.dart';
import 'layout.dart';

class TundHomePage extends StatefulWidget {
  const TundHomePage({super.key});

  @override
  State<TundHomePage> createState() => _TundHomePageState();
}

class _TundHomePageState extends State<TundHomePage> {
  final controller = TundHomeController();

  @override
  void initState() {
    super.initState();
    controller.addListener(onControllerChanged);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      showInitialPrivilegeNotice();
    });
  }

  @override
  void dispose() {
    controller.removeListener(onControllerChanged);
    controller.dispose();
    super.dispose();
  }

  void onControllerChanged() {
    if (!mounted) return;
    setState(() {});
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (controller.logScroll.hasClients) {
        controller.logScroll.jumpTo(
          controller.logScroll.position.maxScrollExtent,
        );
      }
    });
  }

  Future<void> startTund() {
    return controller.startTund(
      confirmPrivileges: confirmPrivileges,
      showError: showError,
    );
  }

  Future<void> copyKey() {
    return controller.copyKey(
      copyText: copyText,
      showError: showError,
      showInfo: showInfo,
    );
  }

  Future<void> copyClientCommand() {
    return controller.copyClientCommand(
      copyText: copyText,
      showInfo: showInfo,
    );
  }

  Future<void> copyText(String text) {
    return Clipboard.setData(ClipboardData(text: text));
  }

  Future<void> showInitialPrivilegeNotice() async {
    if (controller.privilegeNoticeAccepted || !mounted) return;

    final accepted = await showPrivilegeDialog(
      context,
      primaryLabel: 'I understand',
      showCancel: false,
      barrierDismissible: false,
    );
    if (accepted == true && mounted) {
      controller.acceptPrivilegeNotice();
    }
  }

  Future<bool> confirmPrivileges() async {
    if (controller.privilegeNoticeAccepted) return true;

    final accepted = await showPrivilegeDialog(
      context,
      primaryLabel: 'Continue',
      showCancel: true,
      barrierDismissible: true,
    );
    if (accepted == true && mounted) {
      controller.acceptPrivilegeNotice();
      return true;
    }
    return false;
  }

  void showError(String message) {
    showSnackBar(message, TundColors.red);
  }

  void showInfo(String message) {
    showSnackBar(message, TundColors.blue);
  }

  void showSnackBar(String message, Color backgroundColor) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: backgroundColor,
        behavior: SnackBarBehavior.floating,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return TundHomeLayout(
      controller: controller,
      onStart: startTund,
      onStop: controller.stopTund,
      onCopyKey: copyKey,
      onCopyClientCommand: copyClientCommand,
    );
  }
}
