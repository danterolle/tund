import 'dart:convert';

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
    values.sort(
        (a, b) => _ipSortKey(a.virtualIp).compareTo(_ipSortKey(b.virtualIp)));
    return values;
  }

  void clear() {
    _peers.clear();
  }

  bool applyJsonLine(String line) {
    final decoded = jsonDecode(line);
    if (decoded is! Map<String, dynamic>) return false;

    final event = decoded['event'];
    final virtualIp = decoded['virtual_ip'];
    if (virtualIp is! String) return false;

    if (event == 'peer_join') {
      final name = decoded['name'];
      final endpoint = decoded['endpoint'];
      if (name is! String || endpoint is! String) return false;
      _peers[virtualIp] = HostPeer(
        name: name,
        virtualIp: virtualIp,
        endpoint: endpoint,
      );
      return true;
    }

    if (event == 'peer_leave') {
      _peers.remove(virtualIp);
      return true;
    }

    return false;
  }
}

int _ipSortKey(String ip) {
  final parts = ip.split('.');
  if (parts.length != 4) return 0;

  var value = 0;
  for (final part in parts) {
    final octet = int.tryParse(part);
    if (octet == null || octet < 0 || octet > 255) return 0;
    value = (value << 8) | octet;
  }
  return value;
}
