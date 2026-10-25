# Local container deployment

The repository has a multi-stage Dockerfile and a Compose service for the
server. The runtime uses numeric user `65532:65532`, a read-only root
filesystem, no Linux capabilities, no-new-privileges and the checked-in
seccomp allowlist in `docker/seccomp.json`. Temporary token output is written
to a small in-memory filesystem. The container publishes both listeners on
host loopback by default.

## Provide local TLS files

Compose reads three host files from
`%LOCALAPPDATA%\AegisGuard\secrets` by default. The provisioning script creates
this directory with access restricted to the current user:

- `server.pem`: server certificate and any required intermediate
  certificates. Include `localhost` in its subject alternative names so the
  built-in health check can verify TLS.
- `server.key`: matching private key.
- `ca.pem`: CA certificate used to validate the HTTPS listener and
  trusted client certificates.

Run `.\scripts\setup-demo-secrets.ps1` to create demo certificates and keys.
The server configuration expands `${LOCALAPPDATA}` in configured paths.
Compose also accepts `AEGISGUARD_SERVER_CERT_FILE`,
`AEGISGUARD_SERVER_KEY_FILE` and `AEGISGUARD_ADMIN_CA_FILE` overrides for
temporary test files or non-Windows hosts.
Demo keys must never live inside the repository or a cloud-synced folder.

Do not commit real certificates, private keys or tokens. The container
configuration example refers to the Compose-mounted files and writes the
generated demo player token to `/run/aegisguard/player.token`, which is
memory-backed and not persisted. Ensure the host ACLs permit the non-root
container process to read the private key without making a production key
world-readable. Compose's file-backed secrets are local mounts, so verify the
actual host and Docker Desktop permission behavior before using production
credentials.

## Build and run

With Docker Desktop or Docker Engine and Compose installed:

```powershell
docker compose -f docker-compose.yml up --build --detach
docker compose -f docker-compose.yml ps
curl.exe --fail --show-error --cacert "$env:LOCALAPPDATA\AegisGuard\secrets\ca.pem" https://localhost:18444/healthz
```

The game protocol is TCP port 18443. The HTTPS listener is TCP port 18444.
Both host mappings bind to `127.0.0.1`; changing that exposure requires an
explicit deployment decision. `/healthz` is public, while `/status` and
`/metrics` require a trusted client certificate. This Compose file does not
deploy a reverse proxy, Cloudflare Tunnel or public endpoint.

Inspect runtime settings and stop the service with:

```powershell
$container = docker compose -f docker-compose.yml ps -q server
docker inspect $container --format '{{.Config.User}} {{.HostConfig.ReadonlyRootfs}} {{json .HostConfig.CapDrop}} {{json .HostConfig.SecurityOpt}} {{.State.Health.Status}}'
docker compose -f docker-compose.yml down --volumes
```

The image health check makes a verified HTTPS request to `/healthz`. It does
not assert that the game protocol accepts clients or that private status
routes work. `AEGISGUARD_STATUS_HOSTNAME` controls the health check's TLS SNI,
certificate hostname verification and HTTP `Host` header. It defaults to
`localhost` for local certificates; setting it does not change DNS, listener
binding or public exposure. A deployment value must match the origin
certificate.

## CI verification

The `container` job in [CI](../../.github/workflows/ci.yml) builds and starts
the Compose service on Ubuntu, generates short-lived test certificates,
checks the container health and HTTPS health route and asserts the inspected
user, read-only filesystem, capability drop, seccomp and no-new-privileges
settings. It scans the image with Trivy for high and critical OS/library
vulnerabilities and retains inspection and scan output as workflow artifacts.

Docker and Trivy are unavailable on the current Windows workstation. The CI
job is configured but has not run remotely in this work session, so its image
build, health result and vulnerability scan remain unverified until the
workflow runs. No clean scan result is claimed.
