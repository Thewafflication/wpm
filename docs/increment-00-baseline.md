# I-00: WPM 2.0 Baseline and Gap Inventory

**Content type:** Controlled increment record

**Status:** Complete for planning; no product requirement completion is claimed

**Owner:** WPM maintainers

**Date:** 2026-10-11

**Source baseline:** WPM 1.2.8, commit `485501cd940c186aa21498f182dbd499073c360f`

## WSP check

Fetched upstream refs and queried published releases on 2026-10-11. The latest
release and upstream default-branch commit are still WSP 1.4.0 at
`f009399dd1406219571ac978bfee85e85bbdfeac`, already pinned by WPM. No submodule
change is needed. Retain the existing adoption dispositions and unresolved gates.

## Inspection and evidence

Inspected public command dispatch, logging/progress, archive lifecycle, repository
resolution, the accepted ADR-0010/ADR-0013 boundaries, REQ-0014/REQ-0015, and the
complete 2.0 traceability allocation. TC-0014 through TC-0022 describe test plans
but return Blocked for product execution. TC-0023 and TC-0024 through TC-0037 are
registered executable foundations; their results do not prove the remaining
obligations. The 1.2.8 release workflow passed the native x86/x64/ARM64 matrix:
https://github.com/Thewafflication/wpm/actions/runs/38168457193

The table below covers every subordinate obligation in the 2.0 matrix. Each row
inherits its named controlled TC's Fast, PlatformMatrix, Quality,
ManualRealEnvironment, and ReleaseGate allocation. Evidence belongs under the
profile hierarchy in TS-0001; the owner of every row is WPM maintainers. Existing
partial evidence is listed as a starting point, not promoted to Verified.

| Obligation | Delivery increment | Gap disposition | Controlled test | Existing evidence / remaining work |
| --- | --- | --- | --- | --- |
| REQ-0014.001 | I-01 | Partial implementation/evidence | TC-0014 | TC-0027 command inventory and TC-0030 executable nested-action examples produced; recovery workflow demonstration pending |
| REQ-0014.002 | I-01 | Partial implementation/evidence | TC-0014 | TC-0034 package/repository, TC-0036 key/trust, and TC-0037 prerelease-configuration final results produced; remaining mutating-command consistency pending |
| REQ-0014.003 | I-01 | Partial implementation/evidence | TC-0014 | TC-0026 redirected auto/always/never, option-position, and precedence evidence produced; genuine-console and every-class evidence pending |
| REQ-0014.004 | I-01 | Partial implementation/evidence | TC-0014 | TC-0025 known-length HTTP start/completion progress evidence produced; remaining lifecycle phases and bounded long-operation evidence pending |
| REQ-0014.005 | I-01 | Partial implementation/evidence | TC-0014 | TC-0025 verbose credential non-disclosure and TC-0035 install/remove lifecycle, identity, staging, script, retained-artifact, final-result, and environment-secret evidence produced; remaining mutating commands and broader secret classes pending |
| REQ-0014.006 | I-01 | Partial implementation/evidence | TC-0014 | TC-0031 custom-location/level and TC-0032 failure log-path guidance evidence produced; secret inspection pending |
| REQ-0014.007 | I-01 | Partial implementation/evidence | TC-0014 | TC-0026 through TC-0030 cover command, help, option, operand, and nested-action/basic-value partitions; broader semantic values pending |
| REQ-0014.008 | I-01 | Partial implementation/evidence | TC-0014 | TC-0033 package-and-phase delimiters, preserved script standard output/error, contained WPM-looking text, and exit-code-only success/failure evidence produced |
| REQ-0015.001 | I-02/I-03 | Missing product implementation/evidence | TC-0015 | Not yet produced |
| REQ-0015.002 | I-02/I-03 | Missing product implementation/evidence | TC-0015 | Not yet produced |
| REQ-0015.003 | I-02/I-03 | Missing product implementation/evidence | TC-0015 | Not yet produced |
| REQ-0015.004 | I-02/I-03 | Missing product implementation/evidence | TC-0015 | Not yet produced |
| REQ-0015.005 | I-02/I-03 | Missing product implementation/evidence | TC-0015 | Not yet produced |
| REQ-0015.006 | I-02/I-03 | Missing product implementation/evidence | TC-0015 | Not yet produced |
| REQ-0016.001 | I-04/I-05 | Missing product implementation/evidence | TC-0016 | Not yet produced |
| REQ-0016.002 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0024 local fixed/read-only path evidence produced; removable/optical evidence pending |
| REQ-0016.003 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0025 canonical UNC accept, incomplete-UNC reject, and unavailable-UNC (no credential prompt/leak) evidence produced; managed SMB share read/authentication demonstration pending |
| REQ-0016.004 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0025 HTTP default-off, persisted per-repository opt-in, add/refresh security warnings, http-only scope, and credential rejection evidence produced |
| REQ-0016.005 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0024 local unsigned-package policy evidence produced; TC-0025 HTTP unsigned-package rejection evidence produced; SMB/HTTPS negative evidence pending |
| REQ-0016.006 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0024 local effective-source output evidence produced; TC-0025 HTTP/UNC effective-source list/refresh output evidence produced; broader audit/redaction pending |
| REQ-0016.007 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0025 unavailable-UNC-source and cross-origin package-URL rejection evidence produced; managed SMB disconnect/timeout/partial-read and optical-media evidence pending |
| REQ-0016.008 | I-04/I-05 | Partial implementation/evidence | TC-0016 | TC-0024 relative canonicalization, file URL, and device-path evidence produced; TC-0025 UNC and HTTP URL input-partition evidence (incomplete UNC, cross-origin URL, embedded credentials, http-on-https) produced |
| REQ-0017.001 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.002 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.003 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.004 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.005 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.006 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.007 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0017.008 | I-09/I-10 | Missing product implementation/evidence | TC-0017 | Not yet produced |
| REQ-0018.001 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.002 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.003 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.004 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.005 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.006 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.007 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.008 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0018.009 | I-06/I-08 | Missing product implementation/evidence | TC-0018 | Not yet produced |
| REQ-0019.001 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.002 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.003 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.004 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.005 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.006 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.007 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.008 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0019.009 | I-11/I-12 | Missing product implementation/evidence | TC-0019 | Not yet produced |
| REQ-0020.001 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0020.002 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0020.003 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0020.004 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0020.005 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0020.006 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0020.007 | I-13 | Missing product implementation/evidence | TC-0020 | Not yet produced |
| REQ-0021.001 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0021.002 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0021.003 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0021.004 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0021.005 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0021.006 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0021.007 | I-14 | Missing product implementation/evidence | TC-0021 | Not yet produced |
| REQ-0022.001 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.002 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.003 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.004 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.005 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.006 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.007 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.008 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0022.009 | I-15/I-16 | Missing product implementation/evidence | TC-0022 | Not yet produced |
| REQ-0023.001 | I-00 and every increment | Implemented static checks; baseline evidence reconciliation required | TC-0023 | CI evidence not yet produced |
| REQ-0023.002 | I-00 and every increment | Implemented static checks; baseline evidence reconciliation required | TC-0023 | CI evidence not yet produced |
| REQ-0023.003 | I-00 and every increment | Implemented static checks; baseline evidence reconciliation required | TC-0023 | CI evidence not yet produced |
| REQ-0023.004 | I-00 and every increment | Implemented static checks; baseline evidence reconciliation required | TC-0023 | CI evidence not yet produced |
| REQ-0023.005 | I-00 and every increment | Implemented static checks; baseline evidence reconciliation required | TC-0023 | CI evidence not yet produced |
| REQ-0023.006 | I-00 and every increment | Implemented static checks; baseline evidence reconciliation required | TC-0023 | CI evidence not yet produced |

## First-cycle implementation gaps

- I-01: finish typed event/redaction boundaries, local-copy/staging progress,
  missing verbose phases and value partitions, and real-console evidence. Existing
  help, color, logs, script framing, and results should be extended.
- I-02: shared immutable plans are absent. Only upgrade --all has a preliminary
  plan/confirmation; install, remove, selected upgrade, exact fingerprints,
  execution binding, and plan-change rejection remain work.
- I-03: no dry-run capability boundary exists. CLI initialization writes data/logs
  before dispatch, repository resolution refreshes caches, archive validation
  extracts to staging, and scripts/handoffs can execute. Introduce read-only
  resolution and fail-closed mutation guards before exposing the flag.

The interactive self-upgrade prompt fix and the increment plan are incorporated
in this increment's commit. That fix preserves captured handoff output while
preventing background output from overwriting a returned console prompt. Existing
TC-0013 covers captured handoff completion and persistent script/result logging;
interactive-console demonstration remains explicitly outstanding for I-01.

## Change impact and completion checks

Planning/documentation changes preserve all 1.x package, signature, index, and
installed-record formats. The accompanying prompt fix changes interactive
presentation only. No later increment is marked complete. Verification is the
traceability validator plus the mandatory lint/build/full-test commit hooks;
release matrix evidence is retained by this increment's tag-triggered workflow.

I-00 is complete when the inventory uniquely covers the matrix, named controlled
TC files exist, WSP pin/release check is recorded, and validators pass. It does
not require unimplemented I-01 through I-16 product gates to pass. For subsequent
increments, retain an increment record identifying exact evidence and unresolved
requirements rather than claiming a planning allocation is an implementation.
