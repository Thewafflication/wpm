# Blocking WPM 2.0 Quality Candidate Gate

**Content type:** Controlled candidate evidence procedure and contract

**Status:** Implemented gate foundation; product profiles remain incomplete

**Requirements:** REQ-0020.006; REQ-0022.006 and REQ-0022.007

## Required evidence

`tools/quality-candidate-gate.py` aggregates existing evidence without executing
retained commands, changing profiles, or rerunning failures. A complete bundle
contains a UTF-8 `manifest.json` and contained JSON reports for all three native
architectures and these twelve classes:

| Class | Required scenarios |
| --- | --- |
| endurance | install, remove, upgrade |
| archive | malformed, truncated, unsafe-path |
| metadata | malformed, identity-conflict |
| index | malformed, interrupted-publication, prior-cache-preserved |
| signature | invalid, unknown-key, revoked-key |
| path | traversal, reparse, locked |
| self-upgrade | interrupted-handoff, retained-inputs, revalidated-completion |
| recovery | failed-script, tampered-input, safe-retry, safe-cleanup, restore |
| usb | read-only, absent, drive-letter-change, interrupted-read |
| optical | read-only, absent, copied-tree |
| smb | authenticated-share, disconnect, partial-read |
| unavailable-repository | unavailable, prior-cache-preserved |

These 36 architecture/class reports are requirements for a candidate, not
claimed existing passing executions. Query-only fuzz results cannot substitute
for these profiles. Controlled composite TC-0020 remains Blocked until actual
profile producers, runs, bounds, triage, and environment evidence are complete.

The manifest schema is `wpm.quality-candidate.v1`. Required members are
source_revision (exact full Git revision), status (Pass), complete (true),
baseline (exact WSP pin, dependency identity, per-architecture toolchains),
binaries (x86/x64/arm64 SHA-256 values), reports (path/sha256 references), and
findings (explicit array, empty when there are no findings). Report paths are
relative slash-separated contained files. Traversal, absolute paths, reparse
points, symbolic/hard links, control/surrogate path characters, digest changes,
duplicate JSON members and nonfinite
JSON values block. Documents are limited to 4 MiB each, 64 MiB total reads,
128 report references, 1000 cases per profile, and 1000 findings. File reads are
bounded to the inspected size plus one byte; a concurrent size change blocks.

Each report has schema `wpm.quality-profile.v1`, exact source_revision,
source_state clean, architecture, profile, execution native, status Pass,
complete true, matching binary_sha256, nonempty environment, offset-bearing
started/finished timestamps in order, and a baseline object identifying wsp,
dependencies and toolchain, matching the candidate baseline; the WSP value must
match the explicit source gitlink supplied by the caller. Its cases have unique names, status Pass, a recorded
command, integer exit_code and expected_exit_code that agree. All scenarios in
the table must be present; negative tests may have an expected nonzero exit.
USB, optical and SMB also require environment_review references for operator,
procedure and approval. Build-only or emulated runs do not satisfy this native
gate; unsupported equivalence requires a separately approved contract change.

Findings require unique id, owner, affected baseline, requirement, review,
severity, release_effect nonblocking, disposition fixed, correction_revision
matching the candidate, and contained checksummed original_failure and rerun
references. The original must remain Fail at the affected baseline. The rerun
must be one of the validated candidate profiles and Pass at the corrected source.
An unresolved or unknown finding blocks; this tool grants no residual-risk
exception. Original failures cannot be overwritten or relabeled as passes.

The validator checks structure, identity, completeness and integrity. A report
claiming native execution or approval does not itself prove provenance, actual
coverage, authorization, or protected-data safety. The release approver checks
those properties and the controlled readiness record. Synthetic test fixtures
are never valid product or manual-environment evidence.

## Execution and release wiring

```text
python tools/quality-candidate-gate.py --manifest <bundle>/manifest.json --source-revision <exact-revision> --wsp-revision <pinned-wsp-revision>
```

Stdout contains one `wpm.quality-gate.v1` result. Pass returns 0; missing,
malformed, stale, incomplete, non-Pass, unsafe, changed or untriaged input returns
Blocked and exit 1. Stable diagnostic codes contain no raw input paths/content.
Argument-parser failures may precede this result contract.

An exact-source producer run retains the bundle as
`wpm-quality-candidate-input`. Dispatch the Quality candidate gate workflow on
the same source branch/ref with that run ID. It checks the input run's source,
downloads the bundle, validates it, and retains the unchanged reports plus its
own verdict as `wpm-quality-candidate-evidence`, including blocked failures.
Current query corpus runs do not produce this full bundle. Complete profile
producers and controlled real-environment runs are required before a candidate.

For tags beginning `2.` other than explicit `-alpha.` prototypes (including
stable, beta, preview and RC tags), the release workflow first requires
accepted requirements, verified pre-publication traceability, and the latest
exact-source Quality candidate gate run to have succeeded. A later failed,
blocked, queued or running verdict blocks rather than falling back to an earlier
pass. It downloads the retained bundle and reruns aggregation before downstream
verification/build/publication. Missing or expired artifacts block.
Compatible 1.x releases and explicitly incomplete alpha prototypes retain their
existing gates; they do not establish 2.0 acceptance.

REQ-0022.009 requires a retrospective after 2.0.0 publication. The explicit
PrePublication traceability phase permits only that Planned row, with the owned
action in `docs/retrospective-2.0-plan.md`. All other obligations still require
Verified evidence and Accepted parents. The default Complete phase still
requires all 77 obligations Verified, and subsequent stable 2.x releases use it.
This resolves timing; it does not waive a required product result.

## Verification

`tests/verify-quality-candidate-gate.py` uses test-only synthetic bundles to check
the complete matrix and independent rejection partitions, original-failure
retention, corrected results, integrity, malformed input and no evidence writes.
Traceability-validator fixtures verify that pre-publication scheduling cannot
defer runtime work, requires the retrospective owner/action, and cannot satisfy
complete-phase closure. Native CTest runs both foundations. Passing these tests
does not report the candidate's endurance, recovery or physical-media runs Pass.
