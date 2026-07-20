import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/host_peer.dart';

void main() {
  test('tracks peers from host log events', () {
    final tracker = HostPeerTracker();

    tracker.applyLog(
      '[16:03:58] [INFO ] ★ New peer: mylaptop → 10.9.0.2 [192.168.1.8:49599]\n'
      '[16:04:10] [INFO ] ★ New peer: Bridge → 10.9.0.3 [192.168.1.9:49000]\n',
    );

    expect(tracker.peers, hasLength(2));
    expect(tracker.peers[0].name, 'mylaptop');
    expect(tracker.peers[0].virtualIp, '10.9.0.2');
    expect(tracker.peers[0].endpoint, '192.168.1.8:49599');

    tracker.applyLog(
      '[16:05:00] [INFO ] ✦ Peer disconnected: mylaptop (10.9.0.2)\n',
    );

    expect(tracker.peers, hasLength(1));
    expect(tracker.peers.single.name, 'Bridge');
  });

  test('tracks timed out peers from host log events', () {
    final tracker = HostPeerTracker();

    tracker.applyLog(
      '[16:03:58] [INFO ] ★ New peer: Bridge → 10.9.0.3 [192.168.1.9:49000]\n'
      '[16:05:00] [WARN ] ✦ Peer timed out: Bridge (10.9.0.3)\n',
    );

    expect(tracker.peers, isEmpty);
  });
}
