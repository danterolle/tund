# Security Policy

TunD encrypts and authenticates datagrams with a shared key. Packets without the key should not be able to register as peers or inject traffic into the virtual network.

TunD also rejects replayed sequence numbers within a small sliding window for each known remote endpoint.

Traffic is encrypted in transit between each endpoint and the TunD server. It is not end-to-end encrypted against the server itself: the server decrypts packets to route them through the virtual LAN.

Treat TunD as a tool for trusted groups. Do not use it as a general privacy VPN.

## Reporting a vulnerability

Please report security issues privately.

Use GitHub private vulnerability reporting from the repository's **Security** tab when available. If private reporting is unavailable, open a minimal public issue asking for a private contact path.

Do not include sensitive material in a public issue. That includes:

- exploit details
- keys
- crash dumps
- sensitive logs

Include as much as you can share safely:

- Affected version or commit.
- Platform and build method.
- Reproduction steps.
- Expected and observed behavior.
- Impact assessment.

## Disclosure

Please avoid public disclosure until a fix is available or a coordinated disclosure plan has been agreed.

## Operational guidance

- Use a long random shared key. Do not rely on a short memorable password.
- Run TunD only with peers and servers you trust.
- Do not use TunD when you need end-to-end confidentiality from the server.
- Keep Windows release files together, including the bundled `wintun.dll`.
- Restrict inbound UDP exposure to the server port you intend to use.
