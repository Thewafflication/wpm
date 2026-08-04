# REQ-0018: Discoverability and Diagnostics

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 5

## Scope

Applies to read-only health, installed-package, available-package, candidate,
trust, repository, and recovery inspection through diagnose, list, and show.

## Requirement

**REQ-0018.001**
`wpm --diagnose` shall produce a concise health report covering runtime mode,
resolved executable and data locations, configured repository locators and
availability, cache state, trusted and revoked keys, installed-record validity,
architecture conflicts, and pending self-upgrade or recovery work. Diagnosis
shall not initialize or mutate the inspected state.

**REQ-0018.002**
Each unhealthy condition detected by diagnosis shall identify the affected
object or path when safe, explain its operational effect, and provide a
documented next command or action. At minimum, remediation shall cover no
repository, missing trusted key, unavailable repository, invalid installed
record, conflicting architectures, and pending self-upgrade.

**REQ-0018.003**
`wpm list` shall list installed package identities with name, architecture,
current version, retained-version count, and upgrade state without extracting
installed payloads solely for inspection. Invalid or ambiguous records shall
be reported and shall not be silently merged into a valid identity.

**REQ-0018.004**
`wpm list --available` shall list eligible repository packages after repository
priority, availability, architecture, version, and prerelease policy are
applied. It shall distinguish stale/offline information from freshly obtained
repository data and shall not download or install package archives.

**REQ-0018.005**
List commands shall support filters for package name, architecture,
repository, and version with deterministic matching and ordering. An invalid
filter shall fail with the relevant usage and shall not silently broaden the
result set.

**REQ-0018.006**
`wpm show <package>` shall display installed architectures and retained
versions, eligible repository candidates, selected source and repository,
signature trust state, prerelease eligibility, available upgrade, and
relevant audit or recovery information without changing package or repository
state.

**REQ-0018.007**
`wpm show` shall accept `--arch` and `--version` to narrow the view. When a
request remains ambiguous, WPM shall display the available choices and return
the documented non-success selection status rather than silently selecting an
identity, version, architecture, or source.

**REQ-0018.008**
Diagnose, list, list-available, and show shall provide a documented,
versioned machine-readable output mode with deterministic ordering, explicit
null or absence rules, UTF-8 encoding, stable field meanings, and documented
exit semantics. Machine-readable standard output shall contain no color,
progress, prompts, package-script delimiters, or human prose; diagnostics and
logs shall not corrupt it.

**REQ-0018.009**
All inspection output shall redact private keys, credentials, tokens, URL user
information, and avoidable protected data while retaining package, repository,
signer, path, audit, and recovery identities needed for support. Human and
machine views shall be derived from the same read-only query results so their
selection semantics cannot diverge.

## Rationale

Users and support personnel need to understand installed state and candidate
selection without attempting an install or upgrade. A shared read-only query
model prevents diagnostics, tables, and automation from presenting different
answers.

## Verification

**Method:** Automated test, schema validation, inspection, and demonstration

**References:** Planned TC-0018; `docs/traceability-2.0.md`

Planned verification covers healthy and degraded diagnosis, every required
remediation, empty and large listings, filters, offline and stale data,
prerelease and priority selection, corrupt records, multiple architectures,
ambiguous show requests, deterministic machine serialization, redirection,
secret redaction, and before/after no-mutation snapshots.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 5 and ADR-0008.
- **Depends on:** REQ-0010, REQ-0011, REQ-0012, REQ-0013, REQ-0014,
  REQ-0016, ADR-0008, and the DFS logging/protected-data rules.
- **Conflicts with:** None. REQ-0010's existing minimal diagnosis remains a
  compatible subset until implementation of this proposed extension.

## Change Impact

This requirement adds public query commands and a versioned output schema. It
affects installed-record inspection, repository candidate selection, audit and
recovery readers, help, documentation, and compatibility commitments to
automation. It must reuse existing selection logic and ADR-0008's metadata-only
inspection to avoid performance and semantic divergence. No mutation or
package-format change is authorized.

## Tailoring

None. Machine-readable schema changes after acceptance require compatibility
impact analysis and an explicit versioning decision.

## Implementation Record

Planned allocation is one read-only query/health model consumed by table and
versioned machine serializers, with command parsing in the CLI dispatcher and
metadata-only installed-state readers. TC-0018 will verify selection parity,
schemas, remediation, ambiguity, redaction, and no mutation. No implementation
is claimed by this proposed baseline.
