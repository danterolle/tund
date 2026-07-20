# Security Policy

TunD authenticates datagrams with a shared key. Packets without the key should not be able to register as peers or inject traffic into the virtual network.

TunD also rejects replayed sequence numbers within a small sliding window for each known remote endpoint. This is replay protection for TunD datagrams, not traffic confidentiality.

TunD does **not** encrypt packet contents. Treat it as a tool for trusted networks and small trusted groups, not as a privacy VPN. The absence of encryption is a documented limitation, not a vulnerability by itself.

## Reporting a vulnerability

Please report security issues privately.

Use GitHub private vulnerability reporting from the repository's **Security** tab when available. If private reporting is unavailable, open a minimal public issue asking for a private contact path, but do not include exploit details, keys, crash dumps, or sensitive logs in the public issue.

Include as much as you can share safely:

- Affected version or commit.
- Platform and build method.
- Reproduction steps.
- Expected and observed behavior.
- Impact assessment.

## Disclosure

Please avoid public disclosure until a fix is available or a coordinated disclosure plan has been agreed.

## Operational guidance

- Use a long random shared key.
- Run TunD only with peers and servers you trust.
- Do not use TunD on untrusted networks when traffic confidentiality matters.
- Keep Windows release files together, including the bundled `wintun.dll`.
- Restrict inbound UDP exposure to the server port you intend to use.
