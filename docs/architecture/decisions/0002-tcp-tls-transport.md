# ADR 0002: TCP with TLS for the initial game protocol

- Status: Accepted for the initial service
- Date: 2026-10-22

## Context

The first protocol carries compact movement intent. The project has no measured
packet-loss, latency or concurrency requirements. Implementing UDP safely
would require deliberate ordering, replay, congestion and cryptographic
transport decisions.

## Decision

Use an Asio TCP service protected by TLS 1.3 using OpenSSL. Keep the initial
protocol reliable and ordered. Do not add custom transport cryptography or a
custom UDP reliability layer.

## Consequences

- TCP simplifies initial delivery and ordering but can delay later messages
  behind a lost segment.
- TLS adds certificate provisioning and rotation requirements.
- The listener, sequence guard, token bucket and OpenSSL-backed credential store
  are connected and covered by local TLS integration tests.
- Certificate deployment, key rotation, external session issuance and
  representative load testing remain open.
- Revisit transport only after representative gameplay traffic can measure the
  effect of TCP head-of-line blocking.
