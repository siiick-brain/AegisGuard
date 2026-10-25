# CI and software supply chain

## Recorded workflow runs

The repository visibility is **PRIVATE**. `gh run list` and `gh run view`
reported these latest push runs on 2026-10-25 for commit
`ee16ab46c3686ac15366692d90d02010c84aa759`. That commit predates the current
local worktree.

| Workflow | Run | Observed result | Status |
|---|---|---|---|
| [CI](../../.github/workflows/ci.yml) | [40208969572](https://github.com/siiick-brain/AegisGuard/actions/runs/40208969572) | Debug and Release build/test jobs succeeded. The source SBOM step was skipped in Debug and succeeded in Release. No container or skills job appeared. | Verified for the listed jobs only |
| [CodeQL](../../.github/workflows/codeql.yml) | [40208969582](https://github.com/siiick-brain/AegisGuard/actions/runs/40208969582) | Source build succeeded; upload of CodeQL results failed. | **Not verified while the repository is private** |
| [OpenSSF Scorecard](../../.github/workflows/scorecard.yml) | [40208969676](https://github.com/siiick-brain/AegisGuard/actions/runs/40208969676) | The supply-chain analysis step failed; report preservation succeeded. | **Not verified while the repository is private** |
| Container and skills demonstration | No matching job in the latest CI run | No remote container build, Trivy scan or skills report was recorded. | Not verified |
| [Signed server release](../../.github/workflows/release-signing.yml) | No matching run in the latest run list | The workflow requires a pushed `v*` tag. | Not run |

Actions are pinned to commit SHAs. The checked-in source SBOM is not a release
SBOM. It may not inventory system-provided Asio and OpenSSL. The Cosign
workflow's keyless signature will bind a release artifact to its GitHub
workflow identity; this proves build provenance and not a trusted publisher
identity. The signed release workflow has no recorded run.

The CI workflow uses read-only repository permissions. CodeQL and Scorecard
receive only the extra permissions needed to publish code-scanning
results; no credentials are stored in workflow files.

## Deferred tooling and deployment

- The Dockerfile builds the server from Debian packages and Compose supplies
  local TLS files as file-backed secrets. The Trivy job scans OS and library
  vulnerabilities in the built image. Docker and Trivy are unavailable
  locally. The latest remote CI run did not include a container job, so no
  remote image scan is verified.
- `scripts/demo-skills.ps1` records each check in
  `artifacts/skills-report.md`. Without `-Strict`, only a **FAIL** makes the
  script exit non-zero; **BLOCKED** and **Not deployed** remain visible but
  do not affect the exit code. CI runs with `-Strict`, which makes **BLOCKED**
  checks fail the job. **Not deployed** is an informational state and is not
  treated as a pass or as a strict-mode failure.
- The latest local skills report recorded 13 **PASS**, three **BLOCKED**, one
  **Not deployed** and zero **FAIL** results. Normal mode exited with code 0.
- OWASP Dependency-Check remains unconfigured: the project has no supported
  C/C++ lockfile and uses CMake `FetchContent` plus system-installed
  Asio/OpenSSL. There is no infrastructure-as-code target to scan yet.
- Scanner output is retained as a CI artifact. A configured workflow is not a
  passing scan. Report actual results and their scope.

## Reproduce the local build and tests

From the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests/python -p "test_*.py"
```

The recorded GitHub CI run passed Debug and Release build/test jobs on the
pushed commit listed above. Local results do not verify the current unpushed
worktree or jobs that did not appear in that run.

The complete local check sequence is available as:

```powershell
.\scripts\demo-skills.ps1
```

The script exits non-zero for **FAIL** in normal mode; add `-Strict` to make
**BLOCKED** results fail too. **Not deployed** remains an informational state
in either mode and is never counted as **PASS**. Cloudflare remains **Not
deployed** until a domain and active zone are available. See
[the skills evidence map](../skills-evidence.md).

## Framework mapping

- NIST SSDF informs repeatable builds and tests plus static analysis and review
  steps. The presence of these checks is not a maturity or compliance claim.
- CodeQL checks compiled C++ code. Python utility behavior is covered by
  standard-library unit tests. It is not currently included in CodeQL.
- OpenSSF Scorecard evaluates repository and workflow practices. Its latest
  listed run failed during analysis. Scorecard is **Not verified while the
  repository is private** and no score is claimed.
- CycloneDX output provides a source/build inventory. It is not complete
  dependency provenance. CMake's system dependencies remain an inventory
  limitation.
- SLSA provenance and C++ dependency vulnerability checking remain deferred.
  Container hardening is configured but not remotely verified. CIS Benchmark
  review and IaC scanning remain deferred.
