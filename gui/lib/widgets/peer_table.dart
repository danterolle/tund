import 'package:flutter/material.dart';

import '../core/host_peer.dart';
import '../theme.dart';

class TundPeerTable extends StatelessWidget {
  const TundPeerTable({
    super.key,
    required this.peers,
    required this.running,
  });

  final List<HostPeer> peers;
  final bool running;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: TundColors.field.withValues(alpha: 0.42),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: TundColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              const Icon(Icons.lan_rounded, size: 18, color: TundColors.blue2),
              const SizedBox(width: 8),
              const Expanded(
                child: Text(
                  'Connected peers',
                  style: TextStyle(
                    color: TundColors.text,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ),
              Text(
                '${peers.length}',
                style: const TextStyle(
                  color: TundColors.muted,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          if (peers.isEmpty)
            Text(
              running
                  ? 'No peers connected yet.'
                  : 'Start hosting to see connected peers.',
              style: const TextStyle(color: TundColors.faint, fontSize: 12.5),
            )
          else
            SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: DataTable(
                headingTextStyle: const TextStyle(
                  color: TundColors.muted,
                  fontWeight: FontWeight.w800,
                ),
                dataTextStyle: const TextStyle(color: TundColors.text),
                columns: const [
                  DataColumn(label: Text('Name')),
                  DataColumn(label: Text('Virtual IP')),
                  DataColumn(label: Text('Endpoint')),
                ],
                rows: [
                  for (final peer in peers)
                    DataRow(
                      cells: [
                        DataCell(Text(peer.name)),
                        DataCell(Text(peer.virtualIp)),
                        DataCell(Text(peer.endpoint)),
                      ],
                    ),
                ],
              ),
            ),
        ],
      ),
    );
  }
}
