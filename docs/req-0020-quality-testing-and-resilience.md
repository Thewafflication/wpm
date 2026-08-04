# REQ-0020: Tests

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 7; `docs/quality-testing.md`

## Scope

Applies to endurance, fuzzing, malformed-input, fault-injection,
removable-media, transport, recovery, and release-candidate quality testing
that complements the fast deterministic suite.

## Requirement

**REQ-0020.001**
WPM shall maintain a documented, isolated quality-test harness for endurance,
fuzzing, malformed inputs, lifecycle fault injection, removable media,
repository transports, and recovery. The harness shall use disposable data
roots and shall not use production repositories, signing keys, trust stores,
installed packages, or user data.

**REQ-0020.002**
Each quality-test execution shall record the exact command, source and
dependency baseline, environment, architecture, configuration, start and
finish time, controlled status, seed or corpus input, logs, retained artifacts,
and minimized reproduction when available. Protected or secret input shall be
redacted or replaced by test-only material.

**REQ-0020.003**
WPM shall maintain a version-controlled seed and regression corpus for package
archives, metadata, package indexes, signatures, repository indexes and
locators, installed records, command input, configuration, and recovery
records. Every retained corpus item shall identify its purpose, expected
result, origin or generated seed, and affected requirement or threat.

**REQ-0020.004**
Quality tests shall be separate from the fast deterministic pull-request suite
and shall support bounded manual, nightly, prerelease, and disposable
environment execution. Configured time, iteration, disk, memory, process, and
artifact bounds shall cause a controlled failure or inconclusive result rather
than an unbounded hang or host-resource leak.

**REQ-0020.005**
Every discovered defect shall retain its original failing evidence and be
triaged with affected baseline, environment, severity or priority, owner,
requirements, threats, and disposition. A later pass shall not overwrite the
failure. A reproducible failure shall be minimized, and when it can be made
deterministic and fast it shall be promoted into the normal regression suite.

**REQ-0020.006**
Every WPM 2.0 release candidate shall complete the quality-test gate defined in
`docs/quality-testing.md` and record all required execution statuses and
triaged findings. Any required status other than Pass, or an unreviewed finding
whose release effect is unknown, shall block release approval.

**REQ-0020.007**
The quality program shall cover x86, x64, and ARM64 according to the project
test strategy and shall distinguish native execution, approved emulation, and
build-only evidence. Transport/media claims that cannot be reproduced in
isolated automation shall retain controlled real-environment demonstration
evidence and residual limitations.

## Rationale

Long-running, destructive, environmental, and input-space exploration finds
failure modes that do not fit fast CI. Isolation, bounded execution, preserved
failures, and regression promotion turn those findings into reproducible
engineering evidence.

## Verification

**Method:** Harness self-test, controlled failing demonstration, inspection,
and release-record review

**References:** TC-0020; `docs/quality-testing.md`;
`docs/traceability-2.0.md`

Planned verification exercises harness isolation and bounds, corpus metadata,
failure preservation, minimization and promotion workflow, scheduled/manual
entry points, architecture metadata, and a synthetic release-candidate gate
containing Pass and non-Pass results.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 7 and the accepted WPM
  quality-testing program.
- **Depends on:** REQ-0014 through REQ-0019, WSP-TEST-0005 through
  WSP-TEST-0015, and WSP-SEC-0012 through WSP-SEC-0013.
- **Conflicts with:** None. The quality suite complements and does not replace
  deterministic release tests.

## Change Impact

This requirement adds test harnesses, corpus data, scheduled workflows,
retained artifacts, and a release-candidate gate. It may exercise destructive
or network/media fault behavior only inside controlled disposable scopes.
Artifact retention and minimization must prevent untrusted corpus content or
secrets from being published accidentally.

## Tailoring

Individual environmental cases may use approved equivalence or manual
demonstration under `docs/ts-0001-test-strategy.md`; the release-candidate
quality gate and finding triage shall not be omitted.

## Implementation Record

Planned allocation is a project-owned quality harness and corpus beneath
`tests/` with manual/nightly/release-candidate workflow entry points and
controlled completion records. TC-0020 defines harness-control, matrix,
environmental-evidence, and gate-semantics verification; its runner remains
Blocked pending implementation. No harness completion or quality result is claimed by this proposed
baseline.
