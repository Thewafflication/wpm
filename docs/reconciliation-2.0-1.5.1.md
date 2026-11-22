# WPM 2.0 Reconciliation at 1.5.1

**Content type:** Controlled implementation and evidence reconciliation

**Status:** Source/evidence inventory; runtime acceptance remains incomplete

**Owner:** WPM maintainers

**Reconciled product baseline:** `1.5.1` at `998ce8913b371749eff9e82ee2f641c6753aa8c3`

**WSP pin:** `c2dc862e22ccd251be2e32ffba9401496091e598`

**Release evidence:** [run 43245758524](https://github.com/Thewafflication/wpm/actions/runs/43245758524)

This snapshot supersedes the current-status interpretation of the
[1.3.0 reconciliation](reconciliation-2.0-1.3.0.md), which remains an unchanged
historical record. The original planning baseline and increment sequence in
[the work plan](work-plan-2.0.md#61-increment-delivery-plan) are also retained.
This reconciliation changes documentation and evidence allocation only. It
does not implement features, change defaults, accept requirements, or publish
another product release.

## Acceptance accounting

There are still **77 subordinate 2.0 obligations: six Verified static
consistency obligations and 71 Planned runtime obligations**. Planned means
full controlled acceptance is incomplete, not that no implementation exists.
REQ-0014 through REQ-0022 and composite TC-0014 through TC-0022 remain open;
their allocation runners deliberately return Blocked rather than manufacture
passing product evidence. A current native regression pass is foundation
evidence, not proof of their full Fast, PlatformMatrix, Quality,
ManualRealEnvironment and ReleaseGate assertions.

No runtime row is promoted by this review. Legacy requirements gained optional
metadata and cache/history behavior in 1.5.0/1.5.1; those additions do not add to
the separate 77-obligation 2.0 acceptance count. A percentage based on six of
77 would therefore misrepresent implementation progress. No release date is
assigned until the remaining slices and evidence dependencies are scoped.

## Roadmap milestone snapshot

| Roadmap milestone | Shipped foundation at 1.5.1 | Remaining acceptance blocker |
| --- | --- | --- |
| 1. Output/help | Current-command help, color, progress, verbose details, logs and summaries | Complete composite command/profile and protected-data acceptance, including future commands |
| 2. Safer changes | Shared plans, explicit confirmation, no-mutation dry runs, approved-input retention | 2.0 default confirmation, full mutation coverage, remaining commit/phase boundaries |
| 3. Transports | Local/read-only, HTTP opt-in, HTTPS, UNC foundations, identity checks and actionable failures | Physical USB/CD-ROM and authenticated SMB interruption/partial-read/native evidence |
| 4. Authoring | Init, immutable publication, deterministic index, detached sign, explicit verify, copied-tree journey | Repository-scoped configured signer policy, combined indexing/signing, complete findings and real media |
| 5. Discovery | List/show filters, common JSON envelope, runtime/repository queries, optional package metadata | Full health/remediation and selected candidate/trust/upgrade/recovery/ambiguity semantics |
| 6. Recovery | Durable records, inspect, retained inputs, parent/input guards and canonical publication | Complete artifact inventory, supported revalidated retry, conservative cleanup and tested restore |
| 7. Resilience | Query corpus/harness and strict exact-source candidate validator | Actual full producers, bounds, native/media runs and controlled finding review |
| 8. Portability | C99 configuration/lint, helper boundaries, module comments and native builds | Strict portable-core compile gate, remaining OS isolation and complete warning-free API documentation |
| 9. Release experience | Signed multi-architecture 1.x releases, prior-stable upgrades and reviewed PDF | Final 2.0 migration/support/journeys, readiness approval, release record and later retrospective |

These are implementation dispositions, not declarations that a complete
roadmap milestone or a 2.0 release candidate has passed.

## Changes since the 1.3.0 inventory

- The common `--output json` / `wpm.output.v1` read-only envelope exists.
  Remaining REQ-0018 work is full health and selection context plus composite
  acceptance, not creating the envelope again.
- Planned operations write `wpm.recovery.v1`, retain approved inputs and read
  them through held handles. `recovery records` and `recovery inspect`
  revalidate current inputs without mutation. Staging/commit protection and
  owned-file disposition primitives exist. Supported retry/cleanup/restore
  and complete cache/staging/log/handoff inventories remain open.
- The quality candidate validator already requires a strict 36-report matrix
  (12 classes across three architectures), exact source/baseline/digests, and
  reviewed retained findings. The validator and release wiring exist; the
  complete real producer/profile evidence does not. See
  [the candidate contract](quality-candidate-gate.md).
- Installer file-user discovery (1.3.5), confirmed recursive copying (1.3.6),
  and minimum-version dependency resolution (1.4.0) are additional shipped
  product features. Their tests do not substitute for full 2.0 lifecycle,
  safe-default, recovery or physical-environment acceptance.
- 1.4.1 fixes byte-at-a-time HTTP output. 1.5.0 carries optional description,
  tags, upstream repo and other metadata through archives, catalogs and
  queries. 1.5.1 reuses aged valid repository caches, reads local indexes
  directly and restricts WPM history lookup to explicit version selection.
  Their controlled legacy requirements and regression tests provide relevant
  transport/query foundations, not newly Verified 2.0 rows.

## Native evidence, limitations and original failures

The final 1.5.1 release run is successful at the exact product revision above.
Retained [evidence summary](evidence/2.0/1.5.1/release-summary.json), original
TC-0023 reports, extracted full-suite result lines and
[SHA-256 manifest](evidence/2.0/1.5.1/SHA256SUMS) identify this evidence.

| Native verification | Final full-suite result | Source |
| --- | --- | --- |
| x86 | 65 passed, zero failed | Successful original verification job |
| x64 | 64 passed, zero failed | Successful original verification job |
| ARM64 | 63 passed, zero failed | Successful attempt 3 verification job |

Counts differ by architecture/build configuration. The local commit-hook
configuration ran 63 tests. Debug/Release builds, signed package validation,
1.5.0-to-1.5.1 upgrade jobs, documentation/provenance and publication passed.
The optional complete-2.0-candidate gate was **Skipped**, as expected for a
1.x release; it did not provide a 2.0 candidate verdict. Published asset
identities are recorded in the evidence summary.

ARM64 attempt 1 failed in TC-0026 with PowerShell `Internal CLR error.
(0x80131506)`; attempt 2 failed with the same runtime error in
documentation-installation. Original Fail excerpts and job links are retained
separately from attempt 3's successful rerun. No product source/tag change or
test waiver was used to obtain the pass. Different crashing PowerShell tests
suggest runner/runtime instability, but a passing retry is not a proven root
cause or corrective fix. WPM maintainers must triage/reproduce this finding and
record its release effect before relying on affected environments for 2.0
quality/readiness acceptance. This review does not label that finding Fixed.

The six existing Verified REQ-0023 rows now reference fresh native static Pass
reports at 1.5.1 as well as the preserved historical evidence. The documentation
changes made by this reconciliation do not alter accepted ADR/DFS substance,
requirement identities/statuses, runner assertions, product behavior or WSP
pin. Local static/traceability validation checks that documentation-only
equivalence; later behavioral or security changes require renewed review.

## Obligation-by-obligation reconciliation

The status column is formal acceptance, distinct from the shipped scope and
foundation fixtures. Fixture names denote CTest registrations or controlled
test IDs; they do not imply the composite TC has executed its full profiles.

| Obligation | Shipped scope | Foundation evidence | Remaining closure | Acceptance |
| --- | --- | --- | --- | --- |
| REQ-0014.001 | Task help for current commands | TC-0027 | Composite TC-0014 task coverage, including commands added by remaining 2.0 slices. | Planned |
| REQ-0014.002 | Shared semantic events and result summaries | TC-0030/0034/0036/0037; presentation-integration | Complete command/lifecycle composite results and protected-data review. | Planned |
| REQ-0014.003 | Color policy implemented for current output | TC-0026 | Controlled TC-0014 terminal/profile acceptance; retain ARM64 CLR-runner finding. | Planned |
| REQ-0014.004 | Progress across downloads, copying, archives, validation and scripts; bulk HTTP output fixed in 1.4.1 | presentation-integration; http-length-integration; TC-0025 | Full interactive/redirected lifecycle profiles and controlled performance/media evidence. | Planned |
| REQ-0014.005 | Verbose selected-source, phase, path and operation detail | TC-0035; operation-integration | Every mutating command and future retry/cleanup flow under composite secret-safe acceptance. | Planned |
| REQ-0014.006 | Timestamped configurable logs and failure guidance | TC-0031/0032; dry-run-integration | Composite failure/logging review, including complete recovery inventory and no-write paths. | Planned |
| REQ-0014.007 | Narrow action, option and operand diagnostics | TC-0028/0029/0030 | Full controlled current/future command and protected-input profiles. | Planned |
| REQ-0014.008 | Bounded package-script output sections | TC-0033; presentation-integration | Composite lifecycle and redirected stdout/stderr fidelity acceptance. | Planned |
| REQ-0015.001 | Shared install/remove/upgrade plans under explicit --confirm and --dry-run | operation-integration; dry-run-integration; dependency-integration | Make the 2.0 confirmation contract the default, cover every mutation, and accept TC-0015. | Planned |
| REQ-0015.002 | Plans distinguish phases and scheduled self-upgrade from completion | operation-integration; TC-0013; recovery-record-integration | Complete phase/artifact inventory and handoff-completion linkage. | Planned |
| REQ-0015.003 | Safe negative/EOF defaults in explicit confirmation; 1.x defaults preserved | operation-integration; copy-default/legacy-integration | 2.0 default confirmation and complete -y/--yes plan coverage; copy-command tests alone are insufficient. | Planned |
| REQ-0015.004 | Capability-enforced read-only planning and snapshots | readonly-capabilities; dry-run-integration; operation-integration | All supported mutation commands and future retry/cleanup composite coverage. | Planned |
| REQ-0015.005 | CLI rejects unsupported or contradictory planning flags | dry-run-integration; TC-0028/0029 | Complete command matrix and future recovery/cleanup flag combinations. | Planned |
| REQ-0015.006 | Plan digests, retained approved inputs, held-handle staging and canonical retention | operation-integration; admitted-input-integration; guarded-directory-identity | Remaining staging/commit-state boundaries, races and complete fault/native profiles. | Planned |
| REQ-0016.001 | Shared local/web repository, trust and offline foundations; 1.5.1 cache/history policy | linked-repository-integration; resolution-cache-integration; TC-0024/0025 | Full controlled cross-transport contract and native/manual matrix. | Planned |
| REQ-0016.002 | Local/read-only roots, relative paths and source preservation | local-media-integration; TC-0024 | Physical USB/CD-ROM journeys and drive/volume substitution evidence. | Planned |
| REQ-0016.003 | UNC locator/caller-access and failure-handling foundations | TC-0025; local-media-integration | Authenticated native SMB shares, disconnect/timeout/partial-read and protected-share diagnostics. | Planned |
| REQ-0016.004 | Repository-scoped HTTP opt-in, origin restrictions and warnings | TC-0025; linked-repository-integration; http-length-integration | Full interruption, redirect, audit and managed/legacy-environment profiles. | Planned |
| REQ-0016.005 | Independent package trust/signature rules and explicit index-key verification | TC-0012; repository-sign/verify-integration; TC-0025 | Full trust/transport matrix and repository signer policy; transport never grants trust. | Planned |
| REQ-0016.006 | Source locators in plans, output and records with shared filtering | TC-0035; machine-output-integration; operation-integration | Complete recovery artifact/source inventory and protected-data review across all transports. | Planned |
| REQ-0016.007 | Media loss and archive identity replacement fail closed | local-media-integration; operation-integration; admitted-input-integration | Real drive-letter/volume changes, partial reads and SMB interruption evidence. | Planned |
| REQ-0016.008 | Canonical locators, bounded safe links and package containment | linked-repository-integration; TC-0024/0025; repository-health-integration | Complete locator/security partitions in controlled native and real-environment profiles. | Planned |
| REQ-0017.001 | repo init creates controlled layout without destructive reuse | repository-init-integration | Full REQ-0014/0015 authoring plans/confirmation and composite acceptance. | Planned |
| REQ-0017.002 | Validated immutable digest-addressed package publication; optional metadata propagated in 1.5.0 | repository-publish-integration; optional-metadata-integration | Full space/fault/confirmation and native/media acceptance. | Planned |
| REQ-0017.003 | Deterministic package-array rebuilding and atomic publication | repository-index-integration; optional-metadata-integration | Full fault/space/race matrix and controlled acceptance, not reimplementation of indexing. | Planned |
| REQ-0017.004 | Detached explicit repo sign and repo verify --index-key | repository-sign-integration | Persistent repository-scoped signer policy and signing integrated with repo index. | Planned |
| REQ-0017.005 | Read-only explicit catalog/package verification | repository-verify/sign-integration | Report every independently discoverable finding and complete unindexed-file/signature-policy acceptance. | Planned |
| REQ-0017.006 | Authoring dry runs, diagnostics and atomic writes; verification is read-only | repository-init/publish/index/sign/verify-integration | Unify authoring with the full 2.0 operation-plan/default-confirmation contract. | Planned |
| REQ-0017.007 | Copied-tree signed repository round trip without embedded author paths | repository-sign-integration | Physical USB/optical, ACL/read-only and network-share acceptance. | Planned |
| REQ-0017.008 | Documented initialize/add/index/sign/verify/copy/consume journey | docs/repository-authoring.md; repository-sign-integration | Controlled physical-media demonstration and TC-0017 acceptance. | Planned |
| REQ-0018.001 | Runtime diagnosis and separate repository-health queries | runtime-diagnose-integration; repository-health-integration | One complete health report: trust/revocation, installed conflicts, pending handoff and recovery state. | Planned |
| REQ-0018.002 | Source/cache/runtime remediation foundations | repository-health-integration; runtime-diagnose-integration | All specified unhealthy states with a documented safe next action. | Planned |
| REQ-0018.003 | Retained archive inventory without payload extraction | installed-list-integration | Current-version/retained-count/upgrade-state aggregation without claiming deployed state from history alone. | Planned |
| REQ-0018.004 | Read-only catalog inventory with source, priority, staleness and prerelease labels | available-list-integration | Resolver-equivalent eligible candidate selection; inventory labels are not full selection/trust assessment. | Planned |
| REQ-0018.005 | Exact package/architecture/version and catalog-ordinal filters | installed-list-integration; available-list-integration | Composite filter/order/invalid-selection acceptance and final 2.0 query contract. | Planned |
| REQ-0018.006 | Combined retained/catalog show; description/tags/upstream and other metadata added in 1.5.0 | available-list-integration; optional-metadata-integration | Selected source, signature trust, upgrade and audit/recovery context. | Planned |
| REQ-0018.007 | Narrow show selectors and explicit available choices | available-list-integration | The specified non-success ambiguity/selection semantics, not merely multiple inventory rows. | Planned |
| REQ-0018.008 | Common --output json wpm.output.v1 envelope implemented; legacy --json preserved | machine-output-integration; docs/machine-output.md | Full TC-0018 query semantics/profile acceptance; do not list the envelope itself as missing. | Planned |
| REQ-0018.009 | Shared query selection and typed redacted event diagnostics | machine-output-integration; available/installed-list-integration | Full protected-data and human/machine parity review across completed health/selection contexts. | Planned |
| REQ-0019.001 | Atomic wpm.recovery.v1 writer and known phase/input digests for planned operations | recovery-record-integration; operation-integration | Complete cache/staging/log/handoff inventory, interrupted phases and self-upgrade completion linkage. | Planned |
| REQ-0019.002 | recovery records/inspect revalidate retained/current inputs read-only | recovery-record-integration; machine-output-integration | Full artifact/phase inventory, fault/native profiles and TC-0019 acceptance; retryEligible remains false. | Planned |
| REQ-0019.003 | Retained-input and recomputable-plan foundations only; supported recovery retry absent | operation-integration; recovery-record-integration | Implement eligible retry with current identity/trust/state validation and newly approved plans. | Planned |
| REQ-0019.004 | Owned temporary-file disposition primitive; no supported general cleanup command | guarded-directory-identity | Classify active/recoverable/evidence/removable objects; complete planned, idempotent dry-run/confirmed cleanup. | Planned |
| REQ-0019.005 | Held-parent/input guards, canonical retention and owned-file disposal | guarded-directory-identity; admitted-input-integration | End-to-end cleanup retention/active-state policy, races, locks and changed-object fault profiles. | Planned |
| REQ-0019.006 | Lifecycle data and uninstall guidance | docs/lifecycle-data.md; setup.cmd; remove.cmd | Review every retained/removable state class against the completed inventory and cleanup behavior. | Planned |
| REQ-0019.007 | Backup/restore guidance foundation | docs/lifecycle-data.md | Controlled whole-state restore fixtures, quiescence/trust/pending-work validation and partial-restore failures. | Planned |
| REQ-0019.008 | Native foundational recovery tests and retained original/rerun evidence | recovery-record-integration; operation-integration | Full native retry/cleanup/restore/fault acceptance and controlled finding triage. | Planned |
| REQ-0019.009 | Explicit external-script uncertainty and no universal rollback claims | recovery-record-integration; docs/recovery-records.md | Preserve this behavior through implemented retry/cleanup/restore and composite acceptance. | Planned |
| REQ-0020.001 | Isolated bounded query fuzz harness and candidate validator | quality-query-harness; quality-candidate-gate | Actual lifecycle/endurance/archive/signature/transport/media/recovery profile producers and runs. | Planned |
| REQ-0020.002 | Seeded query reports and exact-source candidate manifest validation | quality-query-harness; quality-candidate-gate | Complete controlled environment/baseline/time/command/artifact evidence for each real required profile. | Planned |
| REQ-0020.003 | Version-controlled query corpus and deterministic regression fixtures | tests/quality-query-corpus.json; quality-query-harness | Complete archive/metadata/index/signature/path/configuration/recovery corpus with origins and expected outcomes. | Planned |
| REQ-0020.004 | Bounded manual query workflow and bounded candidate validator | .github/workflows/quality-queries.yml; quality-query-harness | End-to-end OS/resource bounds and manual/nightly/prerelease execution of the full producer suite. | Planned |
| REQ-0020.005 | First-failure retention, regression cases and validator rejection of unreviewed findings | quality-query-harness; quality-candidate-gate; retained ARM64 CLR excerpts | Reviewed controlled findings, owners, release effects and minimized reproductions; retries are not fixes. | Planned |
| REQ-0020.006 | Blocking exact-source 36-report candidate aggregation wired into non-alpha 2.x release | quality-candidate-gate; .github/workflows/quality-candidate.yml | Real complete Pass bundle and reviewed findings; synthetic validator tests and 1.x gate skips are not acceptance. | Planned |
| REQ-0020.007 | Native query workflow and 1.x native release matrix | .github/workflows/quality-queries.yml; 1.5.1 release run | All 12 required quality classes on three architectures, plus controlled USB/optical/SMB environment review. | Planned |
| REQ-0021.001 | C99 language configuration and lexical checks; native builds | tests/lint-c99.ps1; native Debug/Release jobs | Defined portable-core boundary and controlled strict C99 compilation evidence. | Planned |
| REQ-0021.002 | Narrow helpers exist for capabilities, JSON, paths and recovery I/O | wpm/capability.c; wpm/json_input.c; wpm/path_guard.c | Complete OS-interface isolation or reviewed unavoidable exceptions; broad modules still use Windows APIs. | Planned |
| REQ-0021.003 | C99 lexical CI lint and compiler standard configuration | tests/lint-c99.ps1; .github/workflows/release.yml | A controlled portable-core compile gate with newly introduced warnings fatal; lexical lint is insufficient. | Planned |
| REQ-0021.004 | Doxygen-style public comments in current modules | wpm/*.h; source inspection | Complete public-entity contract/ownership/error/security coverage with objective checks. | Planned |
| REQ-0021.005 | Documented security-sensitive helper invariants | wpm/path_guard.h; wpm/recovery_record.h; docs/dfs.md | Full module/data-format coverage and controlled review of remaining boundaries. | Planned |
| REQ-0021.006 | Controlled engineering PDF published | .github/workflows/documentation.yml | Warning-free generated Doxygen API/reference artifact and missing-entity coverage gate; the PDF is not a substitute. | Planned |
| REQ-0021.007 | Regression tests preserve 1.x formats and native behavior | 1.5.1 full native suites; TC-0023 | Accepted portability refactoring and full observable/security/native equivalence evidence. | Planned |
| REQ-0022.001 | 1.x usage/release/backup foundations | docs/usage.md; docs/lifecycle-data.md | 2.0 migration guide with final defaults, preserved formats, backup/recovery and limitations. | Planned |
| REQ-0022.002 | Published 1.5.1 behavior-specific notes | 1.5.1 GitHub release | 2.0 candidate notes covering migration, trust, recovery and limitations, checked against exact binaries. | Planned |
| REQ-0022.003 | Current support policy and controlled engineering PDF | 1.5.1 documentation job | Final 2.0 support/examples/screenshots and completed journey review. | Planned |
| REQ-0022.004 | Native previous-stable 1.5.0-to-1.5.1 upgrade jobs | 1.5.1 Upgrade x86/x64/arm64 jobs | Full 2.0 managed/portable bootstrap, interactive/noninteractive, failed handoff and post-upgrade diagnosis journeys. | Planned |
| REQ-0022.005 | Current 1.x native suites pass: x86 65, x64 64, ARM64 63 tests | docs/evidence/2.0/1.5.1/release-summary.json | Every applicable 2.0 journey/profile, not only ordinary release CI. | Planned |
| REQ-0022.006 | Signed packages, index, documentation provenance and checksums; conditional 2.x candidate gate | 1.5.1 publish job; quality-candidate-gate | All 2.0 safety/recovery/quality and required signing/scanning/provenance/digest gates; candidate gate was skipped for 1.5.1. | Planned |
| REQ-0022.007 | Readiness procedure and candidate validator foundations | docs/quality-candidate-gate.md | Exact-source controlled 2.0 Release Readiness review and all required Pass evidence. | Planned |
| REQ-0022.008 | Tagged 1.5.1 release assets and retained source/evidence identity | docs/evidence/2.0/1.5.1/release-summary.json | Controlled 2.0 Release Record, approval, baseline, digests, findings and support dispositions. | Planned |
| REQ-0022.009 | Owned pre-publication retrospective plan | docs/retrospective-2.0-plan.md | Execute after 2.0.0 publication; PrePublication scheduling exception does not waive other runtime gates. | Planned |
| REQ-0023.001 | Static consistency baseline re-executed on x86/x64/ARM64 at 1.5.1 | TC-0023; retained native reports and SHA256SUMS | Preserve exact/equivalent-baseline static validation as future source/contracts change; no runtime acceptance implied. | Verified |
| REQ-0023.002 | Static consistency baseline re-executed on x86/x64/ARM64 at 1.5.1 | TC-0023; retained native reports and SHA256SUMS | Preserve exact/equivalent-baseline static validation as future source/contracts change; no runtime acceptance implied. | Verified |
| REQ-0023.003 | Static consistency baseline re-executed on x86/x64/ARM64 at 1.5.1 | TC-0023; retained native reports and SHA256SUMS | Preserve exact/equivalent-baseline static validation as future source/contracts change; no runtime acceptance implied. | Verified |
| REQ-0023.004 | Static consistency baseline re-executed on x86/x64/ARM64 at 1.5.1 | TC-0023; retained native reports and SHA256SUMS | Preserve exact/equivalent-baseline static validation as future source/contracts change; no runtime acceptance implied. | Verified |
| REQ-0023.005 | Static consistency baseline re-executed on x86/x64/ARM64 at 1.5.1 | TC-0023; retained native reports and SHA256SUMS | Preserve exact/equivalent-baseline static validation as future source/contracts change; no runtime acceptance implied. | Verified |
| REQ-0023.006 | Static consistency baseline re-executed on x86/x64/ARM64 at 1.5.1 | TC-0023; retained native reports and SHA256SUMS | Preserve exact/equivalent-baseline static validation as future source/contracts change; no runtime acceptance implied. | Verified |


## Recommended remaining delivery sequence

The work plan's I-00 through I-16 identifiers and dependencies remain
authoritative. This is a closure order for already-started slices, not a
claim that earlier partial increments have passed their gates. WPM maintainers
own scoping and evidence review; implementation assignees and estimates remain
unassigned. No agent delegation or product implementation is authorized by
this inventory.

| Priority | Bounded next result | Required demonstration / exit evidence |
| --- | --- | --- |
| 1: I-11 inventory/inspection | Complete cache-preparation, staging, log, retained-input and handoff phase/artifact inventory | Interrupted/failed/completed records; read-only inspection detects absent/changed/untrusted artifacts; phase starts and completions cannot be confused; native and injected-failure evidence |
| 2: I-11 retry | Supported eligible retry derived from that inventory | Recompute current identity, integrity, trust, installed state and plan; new approval; changed/revoked inputs and unsafe script/handoff replay fail closed |
| 3: I-12 cleanup/restore | Conservative object classification, guarded cleanup and validated whole-state restore | No deletion of active/recoverable/required evidence; dry-run/negative confirmation, races, links, locks, repeated cleanup and restore fixtures on all architectures |
| 4: I-02/I-03 and I-06/I-08 closure | Finish 2.0 default-confirmation migration and complete health/selection context | Every mutation and future recovery command has a supported safe plan; complete diagnose/list/show semantics and common JSON parity; preserve compatible 1.x defaults until the 2.0 change is approved |
| 5: I-04/I-05 and I-09/I-10 closure | Finish scoped signer/authoring policy and controlled transport/media journeys | Persistent separate index-signer authority, integrated indexing/signing, complete verification findings, physical USB/optical and authenticated SMB evidence |
| 6: I-13/I-14 closure | Complete real quality producers, finding review, portable-core compile and API gates | All 36 real native class reports with bounds and reviewed failures; strict C99 and warning-free complete API reference evidence |
| 7: I-15/I-16 candidate | Assemble and independently review the exact 2.0 candidate | Accept satisfied requirements and executable composite assertions; full profile matrix, migration/support/bootstrap/upgrade/recovery, readiness, signatures/digests/provenance and release record; retrospective follows publication |

Fixture/corpus drafting, documentation and environmental preparation may begin
earlier, but formal dependencies and acceptance evidence are not waived. Do
not estimate a 2.0 date from the 1.x test count or mark runtime obligations
Verified solely because this inventory now describes their existing code.
