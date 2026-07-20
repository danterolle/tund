class HostPeer {
  const HostPeer({
    required this.name,
    required this.virtualIp,
    required this.endpoint,
  });

  final String name;
  final String virtualIp;
  final String endpoint;
}

class HostPeerTracker {
  final Map<String, HostPeer> _peers = {};

  List<HostPeer> get peers {
    final values = _peers.values.toList();
    values.sort((a, b) => a.virtualIp.compareTo(b.virtualIp));
    return values;
  }

  void clear() {
    _peers.clear();
  }

  void applyLog(String text) {
    for (final line in text.split(RegExp(r'\r?\n'))) {
      final joined = _newPeerPattern.firstMatch(line);
      if (joined != null) {
        final peer = HostPeer(
          name: joined.namedGroup('name')!.trim(),
          virtualIp: joined.namedGroup('vip')!,
          endpoint: joined.namedGroup('endpoint')!,
        );
        _peers[peer.virtualIp] = peer;
        continue;
      }

      final left = _leftPeerPattern.firstMatch(line);
      if (left != null) {
        _peers.remove(left.namedGroup('vip')!);
      }
    }
  }
}

final _newPeerPattern = RegExp(
  r'New peer:\s+(?<name>.+?)\s+.+?\s+(?<vip>\d{1,3}(?:\.\d{1,3}){3})\s+\[(?<endpoint>[^\]]+)\]',
);

final _leftPeerPattern = RegExp(
  r'Peer (?:disconnected|timed out):\s+.+?\((?<vip>\d{1,3}(?:\.\d{1,3}){3})\)',
);
