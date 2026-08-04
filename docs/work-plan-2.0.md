# WPM 2.0 Implementation Work Plan

**Content type:** Controlled work plan

**Status:** Proposed

**Owner:** WPM maintainers

**Planning baseline:** WPM `1.0.16` at `cd0f8ac`

**WSP baseline:** `2198ccab08f969a789448767fe7017b774369adc`

## 1. Objective

Complete the remaining work in `docs/roadmap-2.0.md` while preserving WPM's
1.x package, signature, repository-index, deployment, and upgrade contracts.
Each change package follows the WSP lifecycle: plan, specify, decide/design,
implement, verify, review, and baseline.

## 2. Baseline and planning assumptions

Repository inspection found foundations that should be extended rather than
reimplemented:

- `--verbose`, line-oriented package progress, runtime diagnostics, and
  operational logging already exist but do not meet the full 2.0 criteria.
- HTTPS repositories, signed indexes/packages, offline cache behavior,
  version-aware upgrades, retained archives, and self-upgrade audit logs are
  established 1.x contracts.
- `docs/quality-testing.md` defines the quality-test program, but the harness,
  corpus, scheduled execution, and release-candidate evidence remain work.
- WSP adoption still has deferred release-baseline, release-readiness,
  retrospective, failure-preservation, Debug-artifact/backtrace, and C source
  documentation obligations. Relevant deferred items should be closed by the
  2.0 work rather than tracked in a second parallel plan.
- A roadmap item is not complete until its controlled requirements, design
  effects, tests, traceability, user documentation, and objective evidence are
  complete.

Before starting the first implementation prompt, create a feature branch using
the `codex/` prefix. Use one pull request per change package unless two adjacent
packages are inseparable and the combined review remains manageable.

## 3. Model selection

Use these model profiles throughout the prompt set:

| Profile | Suggested model | Reasoning | Use |
| --- | --- | --- | --- |
| F | GPT-5.6 Sol | high | Architecture, security, compatibility, recovery, and cross-cutting implementation |
| B | GPT-5.6 Terra | medium | Bounded implementation, tests, documentation, and mechanical refactoring |
| R | GPT-5.6 Sol | xhigh | Independent release-gate, security, or complex integration review |

Sol is the default when a task can change package trust, persisted state,
cross-architecture behavior, or compatibility. Terra is appropriate when the
design and pass criteria are already controlled. Increase Sol to `max` only
after a high- or xhigh-reasoning attempt leaves a concrete unresolved defect;
do not select effort solely because a task is large.

## 4. Common prompt contract

Prepend the following contract to every execution prompt below:

```text
Work in the WPM repository and follow the pinned WSP baseline, docs/project-process.md,
docs/ts-0001-test-strategy.md, docs/dfs.md, and existing accepted ADRs. Inspect the
current tree before changing it; preserve unrelated user changes. This is an
implementation task, so make the requested in-scope changes and run relevant
non-destructive validation. Do not change the pinned wsp/ submodule.

For every behavior change: perform and record change-impact analysis; add or update
stable REQ identifiers before implementation; add controlled TC specifications and
automated runners; update bidirectional traceability; preserve 1.x formats and
security rules unless an approved migration explicitly says otherwise. Keep output
scriptable, secrets out of output/logs/artifacts, C code portable to the project's
declared baseline, and Windows behavior valid on x86, x64, and ARM64.

Finish only when implementation, tests, user documentation, traceability, and WSP
dispositions affected by this task agree. Report changed files, commands executed,
results, remaining risks, and any evidence that must run in CI or real hardware.
Do not mark a requirement or roadmap item complete without objective evidence.
```

The prompts intentionally identify outcomes rather than proposed function names.
The implementing agent must inspect the current architecture and keep modules narrow.

## 5. Execution plan and prompts

### CP-00 — Establish the 2.0 controlled baseline

**Depends on:** none  
**Model:** Profile F

#### Prompt CP-00A — Requirements and traceability baseline

```text
Create the controlled WPM 2.0 requirements baseline from docs/roadmap-2.0.md. Inspect
all existing REQ, TC, ADR, DFS, process, and traceability artifacts and do not duplicate
existing obligations. Create cohesive new requirement documents with stable unused
REQ identifiers for all unimplemented 2.0 behavior. Record rationale, applicability,
dependencies, implementation allocation, objective verification methods, and change
impact. Create a 2.0 traceability matrix that can coexist with the 1.0 matrix, and
extend the traceability validator so missing, duplicate, or unverified 2.0 obligations
fail. Update the documentation manifest. This prompt is documentation and validation
only; do not implement product behavior.
```

#### Prompt CP-00B — Architecture and security decisions

```text
Review the new 2.0 requirements against every accepted ADR and docs/dfs.md. Add the
minimum new ADRs needed for output/event presentation, transport-neutral repository
access, repository authoring, dry-run transaction boundaries, machine-readable output,
and recovery records. Prefer extending compatible accepted decisions via a new
superseding or supplemental ADR rather than silently rewriting accepted substance.
Update DFS assets, boundaries, threats, controls, verification, and residual risks for
HTTP opt-in, removable media, authored repository signing, cleanup, and recovery.
Resolve contradictions before implementation and record explicit non-goals.
```

#### Prompt CP-00C — Test allocation and delivery slices

```text
Allocate every new 2.0 requirement to controlled TC specifications using the pinned WSP
template and intentional test-design techniques. Define fast deterministic tests,
platform-matrix tests, quality tests, manual or real-environment tests, evidence paths,
and release gates. Update docs/ts-0001-test-strategy.md where needed. Then update this
work plan with the final REQ/TC identifiers and any dependency corrections discovered.
Do not claim execution results; establish an objectively verifiable test baseline.
```

**Gate:** All roadmap obligations have unique requirements, planned verification,
implementation allocation, and bidirectional traceability.

### CP-01 — Command presentation and help

**Depends on:** CP-00  
**Model:** Profile B; use Profile F for the shared output abstraction if it
crosses security-sensitive script or logging boundaries.

#### Prompt CP-01A — Shared output contract

```text
Implement a single presentation contract for progress, success, warning, error, prompt,
result, and package-script messages. Add --color auto|always|never with accessible
styles, reliable terminal detection, and no escape sequences in redirected output
under auto. Keep non-interactive output stable and line-oriented. Integrate with the
existing WSP-backed operational logger without logging terminal control bytes or
secrets. Add unit/integration coverage for TTY and non-TTY decisions, precedence,
invalid values, and every semantic message class.
```

#### Prompt CP-01B — Help, progress, verbose, and failure logging

```text
Rewrite public command help around common tasks and add short examples for install,
update, upgrade, trust, repository management, and recovery. Improve invalid-argument
errors to show only the relevant usage form. Extend concise progress across repository
access, copy/download, archive, validation, staging, and package-script phases. Audit
every mutating command so --verbose adds selected source/repository, resolved identity,
paths, validation, script, retained-artifact, and final-result detail without repeating
normal output or exposing secrets. Make timestamped operational log verbosity and
location configurable, and point failures to the applicable log. Test every completion
criterion, including package scripts and redirected CI output.
```

**Gate:** Every public command has a tested task example and consistent summaries;
normal, verbose, colored, redirected, and failure output satisfy the roadmap.

### CP-02 — Safe plans, confirmation, and dry runs

**Depends on:** CP-01  
**Model:** Profile F

#### Prompt CP-02A — Plan model and mutation inventory

```text
Design and implement an operation-plan representation for install, remove, and upgrade.
Inventory every mutation to installed payloads, package records, cache, staging, trust,
configuration, repositories, scripts, retained archives, and self-upgrade handoff.
Render the complete plan before confirmation, including identity, architecture, version,
source, affected paths, and whether each item is downloaded, verified, staged, installed,
retained, removed, or deferred until WPM exits. Preserve safe [y/N] defaults and make
-y/--yes the consistent bypass. Clearly bound package-script output.
```

#### Prompt CP-02B — Enforce dry-run purity

```text
Add --dry-run to every supported mutating command using the shared operation plan.
Enforce dry-run below command dispatch so future call sites cannot accidentally mutate.
Dry runs may read and validate but must not change package state, cache, staging, trust,
deployment, config, repository config, audit state, scripts, or self-upgrade handoff.
Build negative tests with before/after filesystem and configuration snapshots, include
failure paths, and prove package scripts are not invoked. Document explicitly unsupported
commands and reject misleading flag combinations.
```

**Gate:** Users see the exact plan before approval and dry-run tests prove zero
durable mutation.

### CP-03 — Transport-neutral repositories

**Depends on:** CP-00, CP-01; CP-02 before install acceptance tests  
**Model:** Profile F

#### Prompt CP-03A — Repository locator and local/read-only media

```text
Implement the approved transport-neutral repository interface while retaining the
existing logical index and signature contracts. Support absolute local paths, drive
roots, USB/removable drives, and CD-ROM/read-only media for repo add, list, update, and
install. Preserve the exact configured locator and report the effective source in
output, diagnostics, verbose logs, and audit records. Define and test absent media,
drive-letter changes, disappearance between phases, read-only sources, path traversal,
and local paths that resemble URLs. Never infer trust from the transport.
```

#### Prompt CP-03B — UNC/SMB and explicit HTTP opt-in

```text
Extend the repository transport interface to UNC/SMB roots and plain HTTP. HTTP must be
disabled by default, require an explicit persisted or per-command opt-in defined by the
requirements, and display an unmistakable transport-security warning without weakening
index/package signature validation. Cover UNC parsing, authentication failures without
credential leakage, disconnects, timeouts, partial reads, redirects, and unsupported
schemes. Test HTTPS regression, HTTP opt-in/warnings, SMB behavior, unavailable sources,
and signature rejection for every transport. Clearly separate mocked deterministic
tests from required managed-network evidence.
```

**Gate:** The same signed repository works from fixed disk, removable/read-only
media, SMB, HTTPS, and opted-in HTTP, with transport-specific failures actionable.

### CP-04 — Repository authoring

**Depends on:** CP-03  
**Model:** Profile F for design/signing; Profile B for docs and bounded tests.

#### Prompt CP-04A — Initialize and add packages

```text
Implement `wpm repo init <directory>` and
`wpm repo add-package <repository> <archive>`. Init must create the approved layout and
local guidance without overwriting an existing repository unexpectedly. Add-package
must validate metadata, archive integrity, package signature/trust policy, duplicate and
conflict behavior, available space, destination containment, and writability before an
atomic copy. Integrate plans, confirmation, --dry-run, verbose output, and audit logging.
Test empty, existing, corrupt, duplicate, malicious-path, insufficient-space, and
read-only repositories.
```

#### Prompt CP-04B — Index, sign, and verify repositories

```text
Implement `wpm repo index <repository>` and `wpm repo verify <repository>`. Generate a
deterministic index from validated archives, atomically replace it, and optionally sign
it with the configured maintainer key without exposing key material. Verify layout,
schema, duplicate identities, entry/archive agreement, availability, package signatures,
index signature, unindexed files according to policy, and path containment. Ensure
verification never mutates. Test deterministic output, interrupted replacement, stale
entries, tampering, wrong keys, read-only verification, and round-trip consumption.
```

#### Prompt CP-04C — Removable-media authoring journey

```text
Document and automate the repeatable journey: initialize on writable local disk, add
packages, build/sign index, verify, copy the complete repository to USB or an optical
mastering directory, make the copy read-only, verify again, configure it, update, and
install. Add a deterministic copy-to-read-only test and specify the manual CD-ROM evidence
required for a release candidate. Confirm that no locator-specific data is embedded in
the generated repository.
```

**Gate:** WPM alone can create, populate, index, sign, verify, copy, and consume a
repository.

### CP-05 — Discoverability and diagnostics

**Depends on:** CP-03; CP-01 output contract  
**Model:** Profile B, escalating to Profile F for selection/ambiguity logic.

#### Prompt CP-05A — Health report and remediation

```text
Expand `wpm --diagnose` into a concise health report covering runtime mode, resolved data
locations, repository locators and availability, cache state, trusted/revoked keys,
installed-record integrity, conflicting architectures, and pending self-upgrade work.
For every unhealthy state, print a documented next action and return the specified stable
exit semantics. Redact secrets and credentials. Add deterministic fixtures for healthy,
degraded, and multiple-failure states and ensure the report is useful when repository
media is absent.
```

#### Prompt CP-05B — List and show queries

```text
Implement `wpm list`, `wpm list --available`, and `wpm show <package>` from a shared
read-only query layer. Provide consistent interactive tables and filters for name,
architecture, repository, and version. Show installed/retained versions, eligible
candidates after repository and prerelease policy, selected source, trust state,
upgrade state, and relevant audit/recovery context. Support --arch and --version;
ambiguous requests must show choices rather than silently selecting. Avoid extracting
installed archives merely to inspect metadata. Test empty, large, ambiguous, corrupt,
offline, prerelease, multi-architecture, and multi-repository states.
```

#### Prompt CP-05C — Stable machine-readable output

```text
Add a documented stable machine-readable mode for diagnose, list, list --available, and
show. Define one versioned schema, deterministic ordering, null/absence rules, encoding,
stdout/stderr separation, and exit behavior. Machine output must contain no color,
progress, prompts, or prose; warnings and logs must not corrupt it. Add schema fixtures,
parser-based tests, redirected-output tests, and compatibility documentation.
```

**Gate:** One diagnose command gathers support data; list/show explain exact package
selection interactively and by a stable machine contract without mutation.

### CP-06 — Recovery and lifecycle cleanup

**Depends on:** CP-02, CP-05  
**Model:** Profile F; Profile R for destructive-boundary review.

#### Prompt CP-06A — Inspect and retry failed operations

```text
Create the approved recovery command surface for interrupted installs, failed package
scripts, and failed self-upgrades. Persist enough non-secret audit identity, phase,
paths, retained artifacts, and retry preconditions to inspect a failure and propose one
safe retry. Revalidate all retained inputs and trust before reuse; never replay a script
or mutable step blindly. Make failures point to inspect/retry/cleanup commands. Add fault
injection at every lifecycle boundary and tests for tampered or missing retained data,
changed repositories, repeated retry, and successful recovery.
```

#### Prompt CP-06B — Safe cleanup and uninstall messaging

```text
Implement a supported cleanup path for stale cache, staging, failed-operation artifacts,
and legacy package records. Classify active versus removable state conservatively, show
the cleanup plan, support --dry-run and confirmation, tolerate races, and never delete
active installations or the only evidence required for a pending recovery. Improve WPM
uninstall messaging to distinguish binaries, mutable shared/user data, retained archives,
configuration, and intentionally preserved recovery evidence. Test symlinks/reparse
points, locked files, partial state, concurrent changes, and idempotence.
```

#### Prompt CP-06C — Backup, restore, and architecture matrix

```text
Document backup and restore expectations for machine and user data roots, including what
must be quiesced, permissions, trust/config implications, pending operations, validation,
and unsupported partial restores. Build automated restore fixtures and execute recovery
coverage on x86, x64, and ARM64. Preserve failing evidence and link the corrective change
and rerun. Update DFS and support guidance with residual risks.
```

**Gate:** Every failed operation identifies retained artifacts and a tested safe next
command; cleanup cannot remove active state; matrix recovery evidence passes.

### CP-07 — Quality testing and resilience

**Depends on:** Feature-specific requirements and interfaces from CP-01 through CP-06  
**Model:** Profile F for harness/fault injection; Profile B for corpus maintenance.

#### Prompt CP-07A — Harness and seed corpus

```text
Implement the program already defined by docs/quality-testing.md. Create an isolated,
disposable quality-test harness for endurance, fuzzing, malformed inputs, fault injection,
removable media, transports, and recovery. Record command, baseline, environment, seed,
corpus item, logs, retained artifacts, status, and minimized reproduction without secrets.
Add a versioned seed/regression corpus covering package metadata, archives, indexes,
signatures, repository locators, installed records, and recovery records. Keep this suite
separate from fast CI and enforce bounded resource use and cleanup.
```

#### Prompt CP-07B — Scheduled and release-candidate gates

```text
Add manual, nightly, and release-candidate quality-test workflows with appropriate
timeouts, architecture/environment selection, artifact retention, and clear failure
summaries. Implement a controlled quality-test completion record that lists every result
and triaged finding. Make non-Pass required results block the release-candidate gate.
Document how to minimize a failure and promote a deterministic fast reproducer into the
normal regression suite while preserving the original failure evidence.
```

**Gate:** Harness and corpus exist; a full release-candidate run produces retained,
triaged evidence and a blocking verdict.

### CP-08 — C99 portability and reference documentation

**Depends on:** Prefer after shared interfaces stabilize in CP-01 through CP-06; may run
in parallel with CP-07  
**Model:** Profile F for portability boundaries; Profile B for documentation coverage.

#### Prompt CP-08A — Portable core and platform helpers

```text
Inventory C11-only language/library use and direct Windows API dependencies in project-
owned C. Define the portable-core boundary and narrow Windows platform helpers without
changing observable behavior or weakening secure path, process, filesystem, console,
network, or cryptographic handling. Refactor in reviewable slices. Add a strict C99
compile configuration with warnings-as-errors for new code and portable-core tests.
Record unavoidable platform dependencies and ensure x86, x64, ARM64, and supported
legacy-Windows assumptions remain explicit.
```

#### Prompt CP-08B — CI compilation and Doxygen reference

```text
Add the C99 compilation check to CI alongside supported Windows builds. Complete
Doxygen-compatible module and public entity contracts, ownership/lifetime rules, data
formats, security boundaries, error behavior, and non-obvious algorithms. Resolve the
deferred WSP C-style documentation dispositions only when automated coverage and review
support the claim. Generate API/reference documentation without warnings, publish it as
a CI artifact when the toolchain is available, and test that broken references or
undocumented required public entities fail the documentation gate.
```

**Gate:** Portable core passes strict C99 compilation; supported Windows builds/tests
remain green; reference docs generate without warnings.

### CP-09 — Release experience and WSP closure

**Depends on:** CP-01 through CP-08  
**Model:** Profile F for integration; Profile R for final readiness review.

#### Prompt CP-09A — Migration, release notes, support, and examples

```text
Write a concise 1.x-to-2.0 migration guide covering preserved contracts, new defaults,
confirmation/dry-run behavior, repository transports and HTTP opt-in, machine output,
recovery/cleanup, data backup, and known limitations. Draft release notes organized by
user-facing behavior, upgrade notes, and limitations. Refresh support policy, usage,
command examples, and any screenshots or rendered examples for 2.0. Validate every
command shown against the release candidate and remove claims not backed by evidence.
```

#### Prompt CP-09B — Bootstrap and self-upgrade journeys

```text
Execute and automate the 2.0 bootstrap installation and self-upgrade journeys from the
previous stable release on x86, x64, and ARM64. Cover machine and portable modes,
interactive and non-interactive paths, signed artifacts, failed handoff/recovery, and
post-upgrade diagnose/list/show behavior. Verify release packages, signatures, version
resources, Defender gates, provenance, and exact digests. Preserve all required evidence
and do not treat build-only cross-compilation as an architecture test pass.
```

#### Prompt CP-09C — Independent release-readiness review

```text
Act as an independent verifier for the WPM 2.0 release candidate. Do not implement fixes
in this pass. Inspect the exact source/WSP/dependency/requirement baselines, traceability,
all matrix and quality-test evidence, unresolved defects/security findings/deviations,
documentation, migration/support/recovery guidance, signing, malware scan, provenance,
and artifact digests. Re-run safe validators where practical. Produce the controlled WSP
Release Readiness record with Pass/Fail/Blocked/Inconclusive statuses and actionable
findings. Any required non-Pass gate blocks approval.
```

#### Prompt CP-09D — Release record and retrospective

```text
After every CP-09C required gate is Pass and the release approver has approved publication,
create the exact WSP release record for WPM 2.0.0: source, requirements, WSP, dependencies,
artifacts, evidence, approvals, support status, and digests. Update the roadmap and WSP
adoption dispositions only where evidence supports completion. After publication, create
the required retrospective and improvement records with owners and effectiveness checks.
Do not publish, tag, sign, or push unless the user separately authorizes those external
or release mutations.
```

**Gate:** All roadmap journeys pass on x86, x64, and ARM64; release readiness is fully
Pass; exact baseline and approval records exist; the retrospective is scheduled or done.

## 6. Recommended pull-request sequence

1. CP-00 controlled baseline.
2. CP-01 presentation/help.
3. CP-02 safe plans/dry-run.
4. CP-03A local media, then CP-03B SMB/HTTP.
5. CP-04 repository authoring.
6. CP-05 diagnostics/discoverability.
7. CP-06 recovery/cleanup.
8. CP-07 quality harness and gates.
9. CP-08 portable core, CI, and reference documentation.
10. CP-09 release experience and closure.

CP-07 harness scaffolding may begin after CP-00, but feature-specific fuzz, fault, and
recovery targets should land with or after their stable interfaces. CP-08 inventory may
begin early; broad refactoring should wait until CP-01 through CP-06 stop moving shared
interfaces.

## 7. Global release gates

WPM 2.0.0 is not ready until:

- every applicable 2.0 and selected WSP requirement has an approved disposition and
  objective verification evidence against the exact or equivalent baseline;
- every mutating command has a safe default, complete plan, consistent non-interactive
  bypass, and tested dry-run behavior where supported;
- local, removable, read-only, SMB, HTTPS, and opted-in HTTP repository journeys pass
  without weakening signature or trust validation;
- deterministic CI, native architecture tests, quality tests, recovery tests, C99 checks,
  reference-documentation checks, release-documentation checks, signing, Defender,
  provenance, and digest gates pass;
- original failures and rerun evidence are preserved and linked;
- migration, support, backup/restore, recovery, and known-limitations documentation agrees
  with the executable; and
- an independent Release Readiness record contains no required non-Pass result.

## 8. Plan maintenance

Update this plan when evidence changes scope, dependencies, risk, or verification. Record
material changes in version control and update requirements, ADR/DFS content, test
allocation, and traceability together. Do not silently delete a prompt after work lands;
mark the corresponding change package complete in the roadmap and link its evidence.
