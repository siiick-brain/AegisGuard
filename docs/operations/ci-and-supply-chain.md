# CI and software supply chain

## Configured checks

The GitHub Actions workflows are configured but have not yet run on GitHub.
Their first successful run is required before treating the checks below as
verified:

| Workflow | Scope | Current status |
|---|---|---|
| [CI](../../.github/workflows/ci.yml) | C++ Debug and Release builds, CTest, Python standard-library tests, and a CycloneDX SBOM artifact from the repository checkout | Configured; remote run pending |
| [CodeQL](../../.github/workflows/codeql.yml) | C++ static analysis on pushes to `main` and a weekly schedule | Configured; remote run pending |
| [OpenSSF Scorecard](../../.github/workflows/scorecard.yml) | Repository and supply-chain posture on pushes to `main` and a weekly schedule; publishes SARIF | Configured; no Scorecard results yet |

Actions are pinned to commit SHAs. The checked-in source SBOM is not a release
SBOM: it may not inventory system-provided Asio and OpenSSL, and it does not
attest to a released binary. Build provenance and artifact signing remain
deferred until the project has a release artifact to attest.

The CI workflow uses read-only repository permissions. CodeQL and Scorecard
receive only the additional permissions needed to publish code-scanning
results; no credentials are stored in workflow files.

## Deferred tooling and deployment

- Trivy and OWASP Dependency-Check are not configured yet. Trivy's documented
  C/C++ filesystem input is `conan.lock`; AegisGuard currently uses CMake
  `FetchContent` and system-installed Asio/OpenSSL, with no C/C++ lockfile,
  container image, or infrastructure-as-code configuration to scan. Adding a
  scanner without an in-scope inventory would give misleading coverage.
- Docker Compose is deferred. The repository currently builds a C++ library
  and test executable, not a runnable server binary with deployment
  configuration. TLS certificate provisioning is also not part of this
  milestone.
- Container, dependency, and infrastructure scans must be added when those
  targets and their reproducible manifests exist. Scanner output must be
  retained and its actual scope stated.

## Reproduce the local build and tests

From the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests/python -p "test_*.py"
```

The GitHub workflow additionally builds and tests a Release configuration.
Local results do not substitute for a completed workflow run.

## Framework mapping

- NIST SSDF informs the repeatable build, test, static-analysis and review
  steps; the presence of these checks is not a maturity or compliance claim.
- CodeQL checks compiled C++ code. Python utility behavior is covered by
  standard-library unit tests; it is not currently included in CodeQL.
- OpenSSF Scorecard evaluates repository and workflow practices. Findings
  must be reviewed and documented after its first run; no score is claimed.
- CycloneDX output provides a source/build inventory, not complete dependency
  provenance. CMake's system dependencies remain an inventory limitation.
- SLSA provenance, dependency vulnerability gating, container hardening and
  CIS Benchmark review remain deferred until a deployable artifact and
  supported dependency manifest exist.
