import 'package:flutter/material.dart';

import '../core/privileges.dart';
import '../core/config.dart';
import '../widgets/widgets.dart';
import 'controller/home_controller.dart';
import 'controls.dart';

class TundHomeLayout extends StatelessWidget {
  const TundHomeLayout({
    super.key,
    required this.controller,
    required this.onStart,
    required this.onStop,
    required this.onCopyKey,
    required this.onCopyClientCommand,
  });

  final TundHomeController controller;
  final Future<void> Function() onStart;
  final VoidCallback onStop;
  final Future<void> Function() onCopyKey;
  final Future<void> Function() onCopyClientCommand;

  bool get running => controller.running;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: TundShell(
        child: LayoutBuilder(
          builder: (context, constraints) {
            if (constraints.maxWidth >= 980) {
              return Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(
                    flex: 6,
                    child: TundCard(
                      child: SingleChildScrollView(child: controls()),
                    ),
                  ),
                  const SizedBox(width: 20),
                  Expanded(
                    flex: 5,
                    child: Column(
                      children: [
                        if (controller.mode == TundMode.server) ...[
                          TundPeerTable(
                            peers: controller.hostPeers,
                            running: running,
                          ),
                          const SizedBox(height: 18),
                        ],
                        Expanded(child: logBox(expanded: true)),
                      ],
                    ),
                  ),
                ],
              );
            }
            return SingleChildScrollView(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  TundCard(child: controls()),
                  const SizedBox(height: 18),
                  if (controller.mode == TundMode.server) ...[
                    TundPeerTable(
                      peers: controller.hostPeers,
                      running: running,
                    ),
                    const SizedBox(height: 18),
                  ],
                  logBox(),
                ],
              ),
            );
          },
        ),
      ),
    );
  }

  Widget controls() {
    return HomeControls(
      status: controller.status,
      guidedError: controller.guidedError,
      privilegeMessage: privilegeMessage(),
      mode: controller.mode,
      running: running,
      showKey: controller.showKey,
      verbose: controller.verbose,
      server: controller.server,
      port: controller.port,
      name: controller.name,
      networkKey: controller.key,
      onModeChanged: controller.setMode,
      onToggleKeyVisibility: controller.toggleKeyVisibility,
      onGenerateKey: controller.generateKey,
      onCopyKey: onCopyKey,
      onCopyClientCommand: onCopyClientCommand,
      onVerboseChanged: controller.setVerbose,
      onStart: onStart,
      onStop: onStop,
    );
  }

  Widget logBox({bool expanded = false}) {
    return TundLogBox(
      text: controller.log.toString(),
      controller: controller.logScroll,
      expanded: expanded,
    );
  }
}
