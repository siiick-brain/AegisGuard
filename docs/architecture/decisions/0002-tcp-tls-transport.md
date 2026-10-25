# ADR 0002: TCP with TLS for the initial game protocol

- Status: Accepted for the initial service
- Date: 2026-10-22

## Context

The first protocol carries compact movement intent. The project has no measured
requirements for packet loss or latency. It has no concurrency targets either.
Implementing UDP safely would require deliberate decisions about ordering and
replay plus congestion and transport cryptography.

## Decision

Use an Asio TCP service protected by TLS 1.3 using OpenSSL. Keep the initial
protocol reliable and ordered. Do not add custom transport cryptography or a
custom UDP reliability layer.

## Consequences

- TCP simplifies initial delivery and ordering but can delay later messages
  behind a lost segment.
- TLS adds certificate provisioning and rotation requirements.
- The listener and sequence guard are connected. The token bucket and
  OpenSSL-backed credential store are also connected. Local TLS integration
  tests cover these components.
- Certificate deployment and key rotation remain open. External session
  issuance and representative load testing remain open too.
- Revisit transport only after representative gameplay traffic can measure the
  effect of TCP head-of-line blocking.
