import 'package:flutter_test/flutter_test.dart';
import 'package:tund_gui/core/host_peer.dart';

void main() {
  test('tracks peers from JSON events', () {
    final tracker = HostPeerTracker();

    expect(
      tracker.applyJsonLine(
        '{"event":"peer_join","name":"mylaptop","virtual_ip":"10.9.0.2","endpoint":"192.168.1.8:49599"}',
      ),
      isTrue,
    );
    expect(
      tracker.applyJsonLine(
        '{"event":"peer_join","name":"Bridge","virtual_ip":"10.9.0.3","endpoint":"192.168.1.9:49000"}',
      ),
      isTrue,
    );

    expect(tracker.peers, hasLength(2));
    expect(tracker.peers[0].name, 'mylaptop');
    expect(tracker.peers[0].virtualIp, '10.9.0.2');
    expect(tracker.peers[0].endpoint, '192.168.1.8:49599');

    expect(
      tracker.applyJsonLine(
        '{"event":"peer_leave","virtual_ip":"10.9.0.2","reason":"disconnect"}',
      ),
      isTrue,
    );

    expect(tracker.peers, hasLength(1));
    expect(tracker.peers.single.name, 'Bridge');
  });

  test('tracks timed out peers from JSON events', () {
    final tracker = HostPeerTracker();

    tracker.applyJsonLine(
      '{"event":"peer_join","name":"Bridge","virtual_ip":"10.9.0.3","endpoint":"192.168.1.9:49000"}',
    );
    tracker.applyJsonLine(
      '{"event":"peer_leave","virtual_ip":"10.9.0.3","reason":"timeout"}',
    );

    expect(tracker.peers, isEmpty);
  });

  test('ignores unsupported JSON events', () {
    final tracker = HostPeerTracker();

    expect(tracker.applyJsonLine('{"event":"unknown","virtual_ip":"10.9.0.2"}'),
        isFalse);
    expect(tracker.peers, isEmpty);
  });

  test('sorts virtual IPs numerically', () {
    final tracker = HostPeerTracker();

    tracker.applyJsonLine(
      '{"event":"peer_join","name":"later","virtual_ip":"10.9.0.10","endpoint":"192.168.1.10:49000"}',
    );
    tracker.applyJsonLine(
      '{"event":"peer_join","name":"first","virtual_ip":"10.9.0.2","endpoint":"192.168.1.2:49000"}',
    );

    expect(
        tracker.peers.map((peer) => peer.virtualIp), ['10.9.0.2', '10.9.0.10']);
  });
}
