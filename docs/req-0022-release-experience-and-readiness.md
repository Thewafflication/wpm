# REQ-0022: Release Readiness

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 9 and Release Gate

## Scope

Applies to migration guidance, release notes, examples, support policy,
bootstrap and prior-stable self-upgrade journeys, release evidence, approval,
publication records, and post-release improvement for WPM 2.0.0.

## Requirement

**REQ-0022.001**
WPM 2.0 shall publish a concise migration guide from supported 1.x releases
that identifies preserved package, signature, repository-index, deployment,
and upgrade contracts; new defaults and command behavior; data or
configuration migration; backup and recovery expectations; compatibility
effects; and known limitations.

**REQ-0022.002**
WPM 2.0 shall publish release notes organized by user-facing behavior,
upgrade/migration notes, security and trust effects, recovery guidance, and
known limitations. Each behavioral claim and command example shall agree with
the release executable and retained verification evidence.

**REQ-0022.003**
The support policy, usage documentation, command examples, rendered
documentation, and screenshots or captured examples shall be reviewed and
updated for 2.0. Unsupported platforms or transports, the HTTP security
warning, package-script trust boundary, no-universal-rollback limitation, and
recovery/support expectations shall remain explicit.

**REQ-0022.004**
The 2.0 release candidate shall complete an isolated bootstrap installation
journey and a self-upgrade journey selected and initiated by the immediately
previous supported stable WPM release on x86, x64, and ARM64. The journeys
shall cover signed artifacts, applicable managed and portable modes,
interactive and non-interactive behavior, failed handoff/recovery, and
post-upgrade diagnosis and package inspection.

**REQ-0022.005**
Every documented 2.0 user journey shall pass on its applicable x86, x64, and
ARM64 release-matrix entries. Native execution or an explicitly approved
emulator shall provide test evidence; cross-compilation alone shall not be
reported as an architecture test pass.

**REQ-0022.006**
Before publication, every mutating command shall have verified safe
confirmation and documented non-interactive behavior; required recovery and
quality-test gates shall pass; and every release package, repository index,
signature, Windows version resource, Authenticode signature and timestamp when
selected, Defender result, provenance attestation, and exact digest shall be
verified against the release candidate.

**REQ-0022.007**
The release approver shall complete a controlled Release Readiness record that
identifies the exact source revision, WSP pin, dependency/toolchain baseline,
requirements baseline and dispositions, artifacts, architecture and quality
evidence, unresolved defects and security findings, deviations, residual
risks, documentation, recovery, support, signing, scanning, provenance, and
digests. Any required status other than Pass shall block a verified release
claim.

**REQ-0022.008**
Publication shall create a controlled Release Record containing approval,
date, tag and source identity, exact requirements/WSP/dependency baseline,
published artifact names and digests, signature/provenance identity, retained
verification locations, approved exceptions, known limitations, and support
status. No requirement or roadmap item shall be marked verified or complete
without linked objective evidence.

**REQ-0022.009**
After the 2.0.0 release, the project shall complete the WSP retrospective and
improvement process. Selected improvements shall identify an owner, intended
result, approval, due or review condition, and effectiveness measure; reusable
process improvements shall be proposed to WSP when appropriate.

## Rationale

A major release is usable only when users can migrate, reproduce documented
commands, recover from failure, and understand support limitations. Exact WSP
baseline, matrix, trust, and artifact records prevent publication from relying
on an unreviewable collection of passing jobs.

## Verification

**Method:** Automated architecture-matrix test, inspection, demonstration,
release-readiness review, and release-record review

**References:** TC-0022; `docs/traceability-2.0.md`; WSP Release
Readiness and Release Record templates

Planned verification executes bootstrap/self-upgrade and documented journeys,
validates command examples and release artifacts, inspects migration/support
content, runs the traceability release-baseline gate, and confirms that the
Release Readiness and Release Records contain exact identities and no required
non-Pass status.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 9 and Release Gate.
- **Depends on:** REQ-0007, REQ-0009, REQ-0013, REQ-0014 through REQ-0021,
  WSP-REQM-0010, WSP-PROC-0007, WSP-PROC-0008, WSP-PROC-0010,
  WSP-TEST-0013, WSP-SIGN-0001 through WSP-SIGN-0018, and
  WSP-WINRES-0001 through WSP-WINRES-0012.
- **Conflicts with:** None identified.

## Change Impact

This requirement controls documentation, support claims, prior-release
compatibility, architecture execution, release workflows, signing/scanning,
evidence retention, approval, and post-release process. It authorizes no tag,
publication, signing, or external release mutation without separate release
approval. Failure of any required gate delays release rather than weakening the
claim.

## Tailoring

Release exceptions require the controlled deviation and approval process in
`docs/project-process.md`. An exception cannot silently convert a non-Pass
required gate into verified evidence.

## Implementation Record

Planned allocation is release documentation, architecture-specific bootstrap
and prior-stable-upgrade runners, artifact validators, controlled readiness and
release records, and a retrospective record. TC-0022 defines aggregation of
journey, matrix, environmental, trust, and record checks without replacing
their underlying evidence; its runner remains Blocked pending implementation. No
release-readiness or completion claim is made by this proposed baseline.
