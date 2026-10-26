# Requirements and milestone gates

This matrix maps project requirements to implementation and verification
evidence. A feature is complete only after its acceptance gate passes.
Milestones 0 through 2 are complete. Milestone 3's intermittent MinGW test
crash was traced to inconsistent Asio thread-backend definitions and fixed in
the current worktree; repeated Debug and Release verification passed.
Milestones 4 through 7 are complete within the protocol and in-memory replay
scopes described below. They also cover synthetic offline movement analysis.
Milestone 8 CI checks are configured but the milestone remains partial. The
latest recorded remote CI run passed Debug and Release build/test jobs and
generated a source SBOM in Release. The CodeQL result upload and Scorecard
analysis failed. No container job appeared in that run. CodeQL and Scorecard
are **Not verified while the repository is private**. Cloudflare remains Not
deployed pending a domain and account access.

| Spec section | Scope and required evidence | Milestone | Status |
|---|---|---:|---|
| 1 | Inspect-first workflow, incremental changes, evidence-based claims, dependency discipline | All | Adopted as project workflow |
| 2 | Select references only when useful; review licenses before adopting code or dependencies | As needed | GoogleTest BSD 3-Clause, Google Benchmark Apache 2.0, Asio BSL-1.0 and OpenSSL Apache 2.0 licenses reviewed |
| 3 | Add components only when their milestone begins; keep a maintainable monorepo | 1 to 11 | In progress |
| 4 | Authoritative C++20 simulation, fixed timestep, testable modules, dynamic Quadtree, staged detector, telemetry, platform security | 1 to 10 | Simulation, Quadtree, server-validated FIRE/health/respawn, and protocol detection are implemented; state replication, persistent telemetry and deployment remain planned |
| 5 | Versioned protocol, bounded parsing and queues, authenticated sessions, replay/rate controls, explicit transport and crypto decisions | 3 | TLS 1.3 listener, session validation, fixed-size parsing, connection cap, deadlines, replay/rate controls and fixed-tick dispatch implemented. MinGW Asio thread-backend definitions were unified to resolve the test teardown crash; 10 full Debug and 10 full Release runs passed. |
| 6 | Validate movement/actions, server-owned state and bounded server-history hit validation | 1, 3 to 4 | Movement and FIRE validation, server-owned health, bounded rewind, hit testing, death and respawn are implemented and covered by tests; client provisioning, outbound replication, static collision and match outcomes remain missing |
| 7 | Quadtree edge/randomized correctness, brute-force oracle, repeatable benchmarks from actual runs | 2 | Core tests and timing benchmarks complete; a query graph records p50/p95/p99 from 20 raw repetitions on eight cases. Service latency and memory measurement remain deferred. |
| 8 | Explainable rules/statistics, behavioral features, evidence, reversible progressive enforcement, seeded synthetic traces | 4, 6 to 7 | Eight protocol/movement/rate-limit signals and an offline movement-repetition heuristic emit explainable findings; recommendations remain observe-only, gameplay analysis and real labeled traces remain planned |
| 9 | Versioned feature pipeline, leakage-safe baseline evaluation, honest held-out metrics; inference only if justified | 6 to 7 | Fixed movement rules and an Isolation Forest baseline are compared on separate seeded synthetic data; neither is deployed for live inference and real-player data is unavailable |
| 10 | Versioned structured telemetry, bounded storage, replay, export, retention, integrity limitations | 5 | Versioned in-memory replay ticks are bounded and tested; persistence, export and cryptographic integrity are deferred |
| 11 | Bounded-cardinality metrics and dashboards reflecting actual services; secure operator UI only if implemented | 9 | Planned |
| 12 | Hardened local containers first; optional cloud/edge/Kubernetes only when justified; placeholder secrets only | 8, 10 | Multi-stage Dockerfile and hardened Compose configuration are present. No container job appeared in the latest remote CI run; Docker and Trivy are unavailable locally. Cloudflare is Not deployed; any future WAF applies to HTTP only, not the game protocol. |
| 13 | Least-privilege CI, build/test/sanitizer/static/dependency/container checks, SBOM and signing only when verifiable | 8 | The latest remote CI run passed Debug and Release build/test jobs and generated a source SBOM in Release. The CodeQL result upload and Scorecard analysis failed. No container job appeared. CodeQL and Scorecard are **Not verified while the repository is private**. Dependency-Check, sanitizers and provenance remain unverified or deferred. |
| 14 | Unit, integration, property, fuzz, adversarial and performance coverage with reproducible configurations | 1 to 10 | Latest Debug, Release and fresh coverage CTest suites pass 125/125; standalone Python discovery passes 14/14 and the CTest pytest suite passes 28 tests with two subtests. Seven RapidCheck properties passed. Fresh measured core coverage is 91.5% lines and 81.0% branches after the current config, aim-grace and reconnect tests. Earlier coverage timeouts did not recur after rebuilding the dedicated coverage tree. A bounded libFuzzer CI job is configured; its run is pending, and Clang is unavailable locally. |
| 15 | Data inventory, minimization, pseudonymization, retention/deletion, access and dataset separation | 5, 7 | Planned |
| 16 | Architecture/data-flow diagrams, STRIDE/attack trees, ADRs, detection matrix, evaluation, operations, privacy, residual-risk documentation | 0 to 11 | Architecture, STRIDE, transport ADR, OWASP checklist and protocol detection notes updated; privacy and operations evidence remain planned |
| 17 | Complete each milestone's acceptance gate before reporting it complete or moving to the next | 0 to 11 | Adopted |
| 18 | Classify every deliverable as implemented, tested, simulated or deferred; make no unsupported claims | All | Adopted |
| 19 | Start with repository/design baseline; agree scope and report plan before broad implementation | 0 | Complete |

## Milestone gates

| Milestone | Exit criteria |
|---|---|
| 0: Repository and design baseline | Requirements traceability, initial architecture/build strategy, documented trust boundaries and risks and the user's approved scope. Complete. |
| 1: C++ foundation and authoritative simulation | Reproducible configure/build/test commands; clean build; tests for state invariants and fixed-timestep behavior. Complete: CMake 4.4.4 configured and built with GCC 15.2.0; all 9 simulation tests passed at that milestone. |
| 2: Spatial engine | Quadtree randomized operations match a brute-force oracle; boundary cases pass; benchmark output is generated by the checked-in harness and records environment/workload. Complete: all 18 tests passed at that milestone; uniform and clustered benchmarks ran with Google Benchmark 1.9.4. Memory measurement remains deferred. |
| 3: Protocol and network hardening | MinGW Asio thread-model ODR mismatch fixed by propagating `ASIO_HAS_PTHREADS=1` consistently from the simulation target. Ten complete Debug and ten complete Release CTest runs passed; TLS lifecycle code was not changed. The optional libFuzzer target was not run because Clang is unavailable. |
| 4: Rule-based detection | Complete for the current protocol surface: eight signals, versioned findings, bounded rolling history and test-only escalating recommendations pass unit and TLS integration tests. Gameplay-behavior rules, persistent evidence and account-level enforcement remain deferred. |
| 5: Telemetry and replay | Complete for caller-managed in-memory replay: versioned ticks can be recorded, validated, replayed and inspected. Limits, data inventory and object-lifetime retention are documented. Network integration, persistence, export and tamper evidence remain deferred. |
| 6: Statistical detection | Versioned replay movement features and configurable thresholds pass reproducible tests on labeled synthetic traces; false-positive risks and the absence of gameplay data are documented. Complete for offline movement analysis only. |
| 7: Synthetic data and ML | Seeded generator compares fixed rules with an Isolation Forest baseline on disjoint synthetic training and evaluation seeds. The model is not deployed because the synthetic data does not establish real-world performance. Complete for synthetic movement traces only. |
| 8: Containers and CI security gates | A multi-stage Dockerfile, hardened Compose file and Linux CI job for build, health, inspect and Trivy are configured. No container job appeared in the latest remote CI run; Docker and Trivy are unavailable locally. The gate remains partial. Dependency-Check is deferred because no supported C++ lockfile exists; build provenance awaits a release artifact. |
| 9: Observability and dashboard | Metrics come from running components, avoid high-cardinality identifiers and are viewable from documented local steps. |
| 10: Load, resilience and security validation | Bounded adverse-load and failure tests produce recorded evidence, documented limits and tracked remediation. |
| 11: Portfolio completion | Required diagrams, reports, operations/privacy docs and demonstration guide match implemented behavior and can be followed from a clean clone. |

## Decisions still deferred

- Production certificate provisioning and key rotation; the TLS service uses
  caller-provided credentials and defaults to loopback.
- Adding an external library or ML model. Cloud service and edge integration
  are deferred too. Kubernetes and Terraform are also deferred.
- Adding a dashboard or persistent event broker.
- Claims of real-world cheat coverage or model accuracy. Production readiness
  and security certification claims are deferred along with performance gains.
- A client kernel driver and collection of indefinite raw input telemetry.

Each deferred choice requires evidence and a documented decision in the
milestone that introduces it.
