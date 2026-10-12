# WPM Quality Testing Program

## Purpose

This program finds reliability, security, and recovery weaknesses that are not
well suited to the fast, deterministic verification suite. It complements CI;
it is not required to run on every commit.

## Test classes

### Stress and endurance

Run repeated install, remove, update, upgrade, repository-index, and
self-upgrade cycles against isolated data roots. Vary package count, archive
size, architecture, retained-version count, and repository availability.

Record duration, failure rate, temporary-file growth, cache growth, handle
leaks where measurable, and whether the final package/audit state is coherent.

### Fuzzing and malformed-input testing

Generate or mutate package archives, package metadata, index JSON, signatures,
repository paths, command-line arguments, and configuration files. Verify WPM
rejects invalid input without crashes, hangs, unsafe path writes, or corrupted
installed state.

Maintain a regression corpus for every discovered failure.

### Fault injection and recovery

Simulate unavailable repositories, removed USB/CD-ROM media, interrupted file
copies, full disks, read-only directories, denied permissions, failed package
scripts, terminated processes, and interrupted self-upgrades. Verify that the
result identifies the failed phase, preserves prior valid state, and offers a
safe retry or cleanup path.

### Transport and media matrix

Exercise HTTPS, explicitly opted-in HTTP, local filesystem, SMB, USB, and
CD-ROM repositories. Cover writable and read-only media, missing media, drive
letter changes, and network-share disconnects.

### Compatibility and performance

Track x86, x64, and ARM64 behavior on supported Windows releases. Run the
separate legacy x86/Windows 2000 investigation described in
[compatibility tracking](compatibility-tracking.md) only when an appropriate
environment is available. Measure large-index and large-package operations to
identify unacceptable regressions.

## Lessons learned

### Interactive prompt tests must distinguish input from EOF

The `upgrade --all` confirmation defect found after WPM 1.0.13 demonstrated
that a negative prompt test can pass for the wrong reason. The original test
piped `n` through PowerShell and observed cancellation, but end-of-file also
selects the safe default and produces the same result. The test therefore did
not prove that WPM had read any standard input. A real user later supplied an
affirmative response and exposed that the CRT input path returned EOF.

For every consequential yes/no prompt:

- test both acceptance and rejection; a rejection-only test is insufficient;
- launch the process with a genuinely redirected standard-input handle, write
  the exact input including its line ending, and close the writer;
- assert observable side effects as well as output and exit status, proving the
  affirmative response was consumed rather than bypassed;
- retain a separate unattended-option test such as `-y`, because bypassing a
  prompt does not exercise prompt input; and
- where console behavior is platform-sensitive, supplement redirected-input
  coverage with a genuine Windows console-input test.

Shell object pipelines are convenient but shall not be treated as proof of
native standard-input behavior unless the test independently demonstrates that
the child consumed the supplied bytes.

## Execution model

- Fast deterministic tests remain mandatory in pull-request and release CI.
- Quality tests may run manually, nightly, before release candidates, or in a
  dedicated environment with disposable virtual machines and data roots.
- Long-running, destructive, network-dependent, and fuzz tests must not use
  production repositories, keys, or user data.
- Failures become tracked issues with their command, seed or corpus input,
  environment, logs, and retained artifacts. Reproducible defects gain a
  minimized regression test in the regular suite where practical.

## Initial release-candidate gate

Before a major release candidate, complete and record at least:

- one endurance run covering install/remove/upgrade cycles;
- malformed archive, metadata, index, signature, and path corpus runs;
- self-upgrade interruption and recovery scenarios;
- removable-media and unavailable-repository scenarios; and
- a review of new crashes, hangs, data-loss risks, and security findings.

## Read-only query corpus (1.2.37)

`tests/quality-query-fuzz.py` provides bounded seeded audit, catalog, and retained
ZIP metadata cases against WPM 1.2.36 or later. It uses fresh explicit roots,
local sources, cached default catalog metadata, synthetic credentials, invalid
HTTPS backend selection, and offline read-only commands. It never installs,
upgrades, removes, extracts, signs, or replays scripts. This is a structural
query corpus, not a replacement for strict invalid-input regression tests or
full archive/trust/transport/recovery testing.

Run from a source checkout using a matching built binary and a fresh output
path. Existing output directories are rejected; the runner never deletes prior
evidence. For example, in PowerShell:

```powershell
python tests/quality-query-fuzz.py `
  --wpm bin/x64/Debug/wpm.exe `
  --output out/quality-local-x64-20040 `
  --cases 1000 --seed 20040 --duration 120 `
  --timeout 10 --max-artifacts-mb 64 `
  --source-revision (git rev-parse HEAD) --source-state dirty
```

Use the actual clean/dirty source state, or leave it unknown. Supplied source
revision/state is declared provenance, not proof that a binary was built from
that tree. The binary SHA-256 identifies the tested executable; retain its
matching runtime and collect `wpm --version` plus pinned dependency/toolchain
baselines separately for a complete controlled execution record.

The first three cases are valid baselines. Later cases deterministically mutate
fields/types, grammar/schema, links, record counts, versions, Unicode/invalid
UTF-8, terminal text, synthetic credentials, audit capacities/exit codes, ZIP
metadata size, and truncation. Cases check coherent JSON/exit/count/metadata-only
flags, no stderr or terminal contamination, no synthetic secret leakage, and
unchanged file contents/hierarchy/timestamps. Snapshot walking rejects reparse
points and files beyond its 2 MiB fixture bound; hashes stream through memory.

The version-controlled `tests/quality-query-corpus.json` records generator,
default seed, family purposes, expected structural invariants, origin, affected
requirements, and limitations. A run retains exact harness/corpus source copies,
a normalized corpus manifest, binary/harness/corpus hashes, platform/Python,
seed, declared source state, command, timing, counts, status, and per-case input
manifests, original fixtures, stdout/stderr bytes, and result records.
`report.json` uses `wpm.quality-query-run.v1`. Reports are staged and flushed
before replacing the previous report; a failed write leaves its candidate for
inspection. Partial setup failures may retain only the last report/inputs.

A failure stops at the first failing case and returns 1 without removing its
evidence. Reproduce with the preserved `harness.py` and adjacent corpus file,
matching executable/runtime, same seed/case count, and a new output directory.
Then minimize the responsible input, file a finding, and promote confirmed
semantic defects into the normal deterministic regression suite. Never replace
an original failed run with a later passing report.

Iteration bounds are 3..10000 cases. Per-process timeout is 1..120 seconds;
whole-run duration is 1..3600 seconds. Captured stdout and stderr are each capped
at 256 KiB and a noisy process is terminated. Timeouts/pipe failures retain the
captured prefix and fail. Duration or artifact budget exhaustion is inconclusive
with exit 1, never Pass. Duration is checked before cases and constrains process
waits; termination/pipe cleanup has bounded additional waits. Artifact budgets
are 1..1024 MiB, checked between cases, so retained evidence may exceed a selected
budget by one bounded case and its final manifest. Defaults are 120 cases,
10 seconds per process, 120 seconds overall, and 64 MiB. These are runner bounds,
not OS-wide RAM/disk quotas or descendant-process containment.

The manual `Quality query corpus` workflow (`quality-queries.yml`) builds and
executes native x86, x64, and ARM64 cases separately on disposable Windows
runners, retaining dependency/runtime/build/query evidence for 90 days. Inputs
are passed as environment values rather than interpolated shell source. A rerun
gets a distinct artifact name containing run ID and attempt. It adds no nightly
schedule and does not relax ordinary release gates. Only the small
`quality-query-harness` self-test joins fast CTest: it proves valid baselines,
real failed subprocess evidence, refusal to overwrite evidence, duration limits,
output caps, and timeout termination.

Local x64 evidence passed a 1000-case seeded run, and x86/x64 each passed
120-case runs and harness self-tests. Native CI evidence remains separate from
manual physical/ACL/deployment coverage. REQ-0020/I-10 remains partial: endurance,
full signatures/archives/paths/configuration, injected lifecycle faults, media,
nightly scheduling, complete environment/resource controls, and the full 2.0
release-candidate quality gate remain open.

Native manual run 38314833741 passed 1000 cases each on x86, x64, and ARM64,
seed 20045, at 751a312eb533b5d1e22b1d666d57106f1c7e485f. Downloaded reports
record clean source state and distinct binary hashes; saved harness/corpus bytes
match their recorded SHA-256 values. Evidence and the local verification record
are retained under out/quality-native-38314833741. This repeats the bounded
structural query subset and does not close the broader quality requirements.

From 1.2.47, a fourth family exercises runtime configuration JSON: backend/CA
selection, invalid combinations, unavailable/long locations, existing Windows
native text, terminal cleanup, and redaction. The three-case minimum remains;
three-case runs retain the original audit/catalog/retained baselines and runs
with four or more cases add the valid runtime baseline before mutations.

Each candidate runs from its owned case directory. Root directory timestamps
and file attributes supplement existing byte/mtime snapshots. Existing input
manifest values retain their two-element shape; additional attributes and
working-directory/environment expectations are separate report fields. Only
generated environment overrides are recorded, not the inherited environment.

Before execution, the harness copies prepared fixtures into a separate
`inputs/case-*` tree, checks byte/time/attribute equality, and records copy
completion and post-process integrity. A process failure leaves those inputs
available even if the working fixture changed. Copies count toward artifact
usage, and existing evidence directories are still refused. Fast tests alter
a failed working input and confirm its preserved copy survives unchanged.
These cooperative checks and copies do not establish an OS security sandbox.

Per-case results remain flushed/staged after every case. The growing run report
is checkpointed every 16 cases and always on failure or termination, avoiding
repeated serialization of the whole history. Interrupted running reports may
lag per-case evidence; they do not establish completion. The original local
1000-case x64 attempt reached its 120-second bound after 498 cases and remains
Inconclusive at out/quality-runtime-tracked-x64-20047; its evidence is retained.
