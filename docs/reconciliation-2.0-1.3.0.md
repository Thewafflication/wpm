# WPM 2.0 Reconciliation at 1.3.0

**Content type:** Controlled implementation and evidence reconciliation

**Status:** Reviewed inventory; runtime acceptance remains incomplete

**Owner:** WPM maintainers

**Source baseline:** 1.3.0 at `192b711` (merged through `9929c7a`)

**Evidence:** https://github.com/Thewafflication/wpm/actions/runs/38328736434

## Scope and evidence rules

Reconcile all 77 subordinate obligations with current source and executable
foundations. Preserve the historical 1.2.8 inventory instead of replacing its
original findings. A compatible 1.x implementation is partial where its
behavior differs from the proposed 2.0 contract. A passing generic native suite
is not evidence for product assertions that have not been implemented.

TC-0023 is already Accepted and registered. Downloaded x86, x64, and ARM64
execution reports record overall Pass, baseline/CRLF equivalence and every
negative fixture. Original report bytes are retained under
`docs/evidence/2.0/1.3.0`, with SHA-256 digests in `SHA256SUMS`.
The workflow identifies the exact tag commit; it also passed the independent
traceability validator and fresh full suite. The reports cover all six static
obligations. Correcting the TC specification's obsolete WSP assumption to the
1.4.0 pin used by the runner changes documentation only. The upcoming envelope
adds behavior governed by ADR-0010 without altering accepted ADR/DFS substance
or the pinned WSP revision. Re-run static validation on the new baseline before
merging; future architecture changes invalidate this equivalence if their
controlled relationships or assertions change.

Runtime obligations remain Planned until their controlled composite runners,
full assertions, profiles, and exact evidence satisfy acceptance. There are six
Verified static obligations and 71 runtime obligations awaiting closure.

## Requirement-by-requirement allocation

Each runner below supplies partial foundation evidence; the closure column
identifies work needed in addition to the requirement's full text and controlled
TC. Requirement-specific prior findings remain in `traceability-2.0.md`.

| Obligation | Existing implementation | Executable foundations | Remaining closure |
| --- | --- | --- | --- |
| REQ-0014.001 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.002 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.003 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.004 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.005 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.006 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.007 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0014.008 | wpm/logging.c; wpm/progress.c; CLI help | TC-0026 through TC-0037; presentation-integration | Current commands covered in I-01; include future commands and composite protected-data/event review |
| REQ-0015.001 | wpm/operation.c; wpm/capability.c | operation-integration; readonly-capabilities; dry-run-integration; TC-0013 | 1.x confirmation remains opt-in; restore safe 2.0 defaults and finish composite profiles |
| REQ-0015.002 | wpm/operation.c; wpm/capability.c | operation-integration; readonly-capabilities; dry-run-integration; TC-0013 | 1.x confirmation remains opt-in; restore safe 2.0 defaults and finish composite profiles |
| REQ-0015.003 | wpm/operation.c; wpm/capability.c | operation-integration; readonly-capabilities; dry-run-integration; TC-0013 | 1.x confirmation remains opt-in; restore safe 2.0 defaults and finish composite profiles |
| REQ-0015.004 | wpm/operation.c; wpm/capability.c | operation-integration; readonly-capabilities; dry-run-integration; TC-0013 | 1.x confirmation remains opt-in; restore safe 2.0 defaults and finish composite profiles |
| REQ-0015.005 | wpm/operation.c; wpm/capability.c | operation-integration; readonly-capabilities; dry-run-integration; TC-0013 | 1.x confirmation remains opt-in; restore safe 2.0 defaults and finish composite profiles |
| REQ-0015.006 | wpm/operation.c; wpm/capability.c | operation-integration; readonly-capabilities; dry-run-integration; TC-0013 | 1.x confirmation remains opt-in; restore safe 2.0 defaults and finish composite profiles |
| REQ-0016.001 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.002 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.003 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.004 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.005 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.006 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.007 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0016.008 | wpm/repository.c; wpm/https.c | local-media-integration; linked-repository-integration; http-length-integration; TC-0024/TC-0025 | Real USB/optical and authenticated SMB interruption evidence remains open |
| REQ-0017.001 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.002 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.003 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.004 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.005 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.006 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.007 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0017.008 | wpm/repository.c; wpm/signing.c | repository-init/publish/index/sign/verify-integration | Persistent repository-scoped signer policy, combined indexing/signing, complete findings, and real media evidence remain open |
| REQ-0018.001 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.002 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.003 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.004 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.005 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.006 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.007 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.008 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0018.009 | wpm/wpm.c runtime query; wpm/repository.c shared package queries | runtime-diagnose-integration; installed-list-integration; available-list-integration; repository-health-integration | Full health/remediation, resolver eligibility/trust/upgrade/recovery context, ambiguous status, and wpm.output.v1 remain open |
| REQ-0019.001 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.002 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.003 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.004 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.005 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.006 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.007 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.008 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0019.009 | wpm/recovery.c; docs/lifecycle-data.md | recovery-integration; TC-0007/TC-0013 | Versioned retained-input recovery records, safe retry, conservative cleanup, and full restore matrix remain open |
| REQ-0020.001 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0020.002 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0020.003 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0020.004 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0020.005 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0020.006 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0020.007 | tests/quality-query-fuzz.py; tests/quality-query-corpus.json | quality-query-harness; quality-query.yml | Query-only coverage; lifecycle/endurance/transport profiles, OS bounds, finding triage and blocking candidate gate remain open |
| REQ-0021.001 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0021.002 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0021.003 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0021.004 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0021.005 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0021.006 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0021.007 | tests/lint-c99.ps1; existing module comments | C99 lexical lint; native Debug/Release builds | Strict warning-as-error C99 compile, narrow platform interfaces and warning-free complete Doxygen reference remain open |
| REQ-0022.001 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.002 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.003 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.004 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.005 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.006 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.007 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.008 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0022.009 | install.ps1; .github/workflows/release.yml; docs/support-policy.md | 1.3.0 native release/bootstrap/previous-stable upgrade workflow | 2.0 migration/support examples, complete candidate profiles and exact readiness/release/retrospective records remain open |
| REQ-0023.001 | accepted ADR/DFS/requirements; tests/tc-0023-architecture-security-consistency.ps1 | TC-0023; verify-traceability-validator.ps1 | Static baseline verified at 1.3.0; no runtime feature completion claimed |
| REQ-0023.002 | accepted ADR/DFS/requirements; tests/tc-0023-architecture-security-consistency.ps1 | TC-0023; verify-traceability-validator.ps1 | Static baseline verified at 1.3.0; no runtime feature completion claimed |
| REQ-0023.003 | accepted ADR/DFS/requirements; tests/tc-0023-architecture-security-consistency.ps1 | TC-0023; verify-traceability-validator.ps1 | Static baseline verified at 1.3.0; no runtime feature completion claimed |
| REQ-0023.004 | accepted ADR/DFS/requirements; tests/tc-0023-architecture-security-consistency.ps1 | TC-0023; verify-traceability-validator.ps1 | Static baseline verified at 1.3.0; no runtime feature completion claimed |
| REQ-0023.005 | accepted ADR/DFS/requirements; tests/tc-0023-architecture-security-consistency.ps1 | TC-0023; verify-traceability-validator.ps1 | Static baseline verified at 1.3.0; no runtime feature completion claimed |
| REQ-0023.006 | accepted ADR/DFS/requirements; tests/tc-0023-architecture-security-consistency.ps1 | TC-0023; verify-traceability-validator.ps1 | Static baseline verified at 1.3.0; no runtime feature completion claimed |

## Closure sequence and change impact

1. Establish the common `--output json` envelope through shared query results
   and typed, redacted diagnostics; retain existing `--json` schemas unchanged.
2. Complete full diagnose health/remediation and list/show eligibility,
   trust, upgrade, recovery context, and ambiguity semantics.
3. Implement versioned retained-input recovery inspection before retry or
   cleanup; finish their guarded plans and restore matrix.
4. Close real transport/media evidence and repository signer/authoring policy.
5. Expand quality profiles and the blocking candidate verdict, then strict C99
   boundaries/reference documentation and complete candidate journeys.
6. Reconcile every controlled product TC, accept satisfied requirements,
   run the exact native/profile matrix, and review release readiness.

The first change corrects stale planning and test assumptions and retains
objective static evidence. It changes no product defaults, formats, trust,
mutation authority, dependencies, or support scope. Envelope work separately
updates REQ-0018's implementation allocation and schema test specification before
implementation. No runtime milestone or complete 2.0 release is declared here.
