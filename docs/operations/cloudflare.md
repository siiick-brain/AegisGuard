# Cloudflare deployment boundary

## Status

**Not deployed.** A Cloudflare account dashboard URL was supplied, but no
registered domain or active zone is confirmed. No Tunnel or Terraform plan has
been supplied or applied. No public endpoint is configured. These notes define
scope and future inputs; they are not evidence of Cloudflare protection.

## HTTP-only scope

The only candidate Cloudflare surface is the separate HTTPS listener on port
18444:

- `GET /healthz` is public and intended for health probes.
- `GET /status` and `GET /metrics` require a trusted client certificate at
  the server.

Any future Tunnel, Full (strict) TLS configuration, WAF custom rule and
rate-limit rule must target this HTTP(S) surface only. The game service on TCP
port 18443 uses a custom binary protocol. It is not HTTP and is not protected
by Cloudflare HTTP WAF or HTTP rate-limiting rules. Do not route or describe
the game protocol as protected by those controls.

The current Compose host bindings are loopback-only. A future Tunnel must be
explicitly connected to the HTTPS listener without publishing it publicly.
Origin certificate verification must remain enabled. Do not assume edge
client-certificate validation is equivalent to the server's mTLS check:
`/status` and `/metrics` must continue to receive a trusted client certificate
at the application, or remain unavailable through the Tunnel until that
behavior is verified.

## Domain-agnostic local configuration

Compose passes `AEGISGUARD_STATUS_HOSTNAME` to the server health-check command.
It selects the TLS SNI name, certificate hostname check and HTTP `Host` value
for the local `/healthz` probe. Compose defaults it to `localhost`, so local
builds and tests need no public hostname, DNS record or Cloudflare account.
Changing it does not bind the listener to a DNS name, create DNS or expose a
public endpoint. Use a deployment hostname only after its origin certificate
contains that name and the Tunnel route has been explicitly configured.

The future deployment inputs are expected to include:

- `AEGISGUARD_STATUS_HOSTNAME`: the operator-selected HTTPS hostname. It has
  no public default.
- `CLOUDFLARE_ACCOUNT_ID` and `CLOUDFLARE_ZONE_ID`: account and active-zone
  identifiers for the selected domain.
- `CLOUDFLARE_API_TOKEN`: a least-privilege token held only in a local
  environment or CI secret.
- A Cloudflare Tunnel identifier and its runtime credential, provisioned or
  supplied after the origin and deployment model are chosen.

No hostname, zone, Tunnel or token is configured by this repository today.

## Planned policy and evidence

After a hostname and Cloudflare account are available, create Terraform for:

- A proxied DNS name attached to a Cloudflare Tunnel.
- Full (strict) TLS for the HTTP origin, with origin certificate validation.
- A WAF custom rule scoped to the status host and intended HTTP paths.
- A rate-limit rule scoped to the HTTP endpoint, with thresholds recorded
  from an explicit operational decision.

No Terraform is applied and no `terraform plan` output exists. Deployment
evidence must include a real plan, a successful `curl` response containing
`cf-ray`, a request blocked by the WAF rule and a request returning HTTP 429
after the configured limit. Redact identifiers as appropriate and never put
Cloudflare API tokens in the repository, command output or logs.

## Inputs needed before deployment

1. The domain must be registered and active in Cloudflare, and the desired
   status hostname must be identified.
2. The Cloudflare account and active zone must be accessible to the operator.
3. The origin/Tunnel location and DNS ownership must be confirmed.
4. A least-privilege Cloudflare API token must be configured locally as
   `CLOUDFLARE_API_TOKEN` or stored as a CI secret. Do not send the token in
   chat or commit it.
5. The operator must approve the exact WAF expression, rate threshold and
   behavior for the public health route.

Until these inputs and tests are available, Cloudflare remains **Not
deployed**.
