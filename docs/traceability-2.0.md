# WPM 2.0 Requirements Traceability

**Content type:** Controlled traceability matrix

**Status:** Proposed

**Baseline:** Proposed WPM 2.0 runtime requirements REQ-0014 through REQ-0022
and accepted architecture-consistency requirement REQ-0023

This matrix allocates every identified 2.0 subordinate obligation to a planned
controlled test case and verification method. `Planned` means the requirement
and verification allocation are controlled but no passing implementation
evidence is claimed. A row may become `Verified` only after the referenced test
specification, automated runner or controlled non-test method, and retained
objective evidence exist for the exact or configuration-equivalent baseline.

Normal validation checks complete, unique, bidirectional allocation. Release
validation runs `tests/verify-traceability.ps1 -ReleaseBaseline 2.0` and rejects
every row that is not `Verified` or lacks objective evidence.

| Requirement | Test case | Verification method | State | Evidence |
| --- | --- | --- | --- | --- |
| REQ-0014.001 | TC-0014 | Automated test and inspection | Planned | TC-0027 command inventory and TC-0030 executable nested-action examples produced; recovery workflow demonstration pending |
| REQ-0014.002 | TC-0014 | Automated test and inspection | Planned | TC-0034 install/remove identity-qualified final-result evidence produced; remaining mutating-command consistency pending |
| REQ-0014.003 | TC-0014 | Automated test and demonstration | Planned | TC-0026 redirected auto/always/never, option-position, and precedence evidence produced; genuine-console and every-class evidence pending |
| REQ-0014.004 | TC-0014 | Automated test | Planned | Not yet produced |
| REQ-0014.005 | TC-0014 | Automated test and secret-redaction inspection | Planned | Not yet produced |
| REQ-0014.006 | TC-0014 | Automated test and inspection | Planned | TC-0031 custom-location/level and TC-0032 failure log-path guidance evidence produced; secret inspection pending |
| REQ-0014.007 | TC-0014 | Automated negative test | Planned | TC-0026 through TC-0030 cover command, help, option, operand, and nested-action/basic-value partitions; broader semantic values pending |
| REQ-0014.008 | TC-0014 | Automated redirected-output test | Planned | TC-0033 package-and-phase delimiters, preserved script standard output/error, contained WPM-looking text, and exit-code-only success/failure evidence produced |
| REQ-0015.001 | TC-0015 | Automated test and inspection | Planned | Not yet produced |
| REQ-0015.002 | TC-0015 | Automated lifecycle test | Planned | Not yet produced |
| REQ-0015.003 | TC-0015 | Automated input-partition test | Planned | Not yet produced |
| REQ-0015.004 | TC-0015 | Automated state-difference test | Planned | Not yet produced |
| REQ-0015.005 | TC-0015 | Automated negative test | Planned | Not yet produced |
| REQ-0015.006 | TC-0015 | Automated state-transition test | Planned | Not yet produced |
| REQ-0016.001 | TC-0016 | Integration test and inspection | Planned | Not yet produced |
| REQ-0016.002 | TC-0016 | Automated filesystem/media test | Planned | TC-0024 local fixed/read-only path evidence produced; removable/optical evidence pending |
| REQ-0016.003 | TC-0016 | Automated test and managed SMB demonstration | Planned | TC-0025 canonical UNC accept, incomplete-UNC reject, and unavailable-UNC (no credential prompt/leak) evidence produced; managed SMB share read/authentication demonstration pending |
| REQ-0016.004 | TC-0016 | Automated policy and warning test | Planned | TC-0025 HTTP default-off, persisted per-repository opt-in, add/refresh security warnings, http-only scope, and credential rejection evidence produced |
| REQ-0016.005 | TC-0016 | Automated signature/trust negative test | Planned | TC-0024 local unsigned-package policy evidence produced; TC-0025 HTTP unsigned-package rejection evidence produced; SMB/HTTPS negative evidence pending |
| REQ-0016.006 | TC-0016 | Automated output/audit inspection | Planned | TC-0024 local effective-source output evidence produced; TC-0025 HTTP/UNC effective-source list/refresh output evidence produced; broader audit/redaction pending |
| REQ-0016.007 | TC-0016 | Fault injection and media demonstration | Planned | TC-0025 unavailable-UNC-source and cross-origin package-URL rejection evidence produced; managed SMB disconnect/timeout/partial-read and optical-media evidence pending |
| REQ-0016.008 | TC-0016 | Automated input-partition test | Planned | TC-0024 relative canonicalization, file URL, and device-path evidence produced; TC-0025 UNC and HTTP URL input-partition evidence (incomplete UNC, cross-origin URL, embedded credentials, http-on-https) produced |
| REQ-0017.001 | TC-0017 | Automated state-transition test | Planned | Not yet produced |
| REQ-0017.002 | TC-0017 | Automated negative and fault test | Planned | Not yet produced |
| REQ-0017.003 | TC-0017 | Automated determinism and atomicity test | Planned | Not yet produced |
| REQ-0017.004 | TC-0017 | Automated signing test and security inspection | Planned | Not yet produced |
| REQ-0017.005 | TC-0017 | Automated validation test | Planned | Not yet produced |
| REQ-0017.006 | TC-0017 | Automated plan/dry-run test | Planned | Not yet produced |
| REQ-0017.007 | TC-0017 | Automated copy/read-only test and demonstration | Planned | Not yet produced |
| REQ-0017.008 | TC-0017 | Documentation inspection and journey demonstration | Planned | Not yet produced |
| REQ-0018.001 | TC-0018 | Automated health-state test | Planned | Not yet produced |
| REQ-0018.002 | TC-0018 | Automated condition/remediation test | Planned | Not yet produced |
| REQ-0018.003 | TC-0018 | Automated installed-state test | Planned | Not yet produced |
| REQ-0018.004 | TC-0018 | Automated repository-selection test | Planned | Not yet produced |
| REQ-0018.005 | TC-0018 | Automated filter-partition test | Planned | Not yet produced |
| REQ-0018.006 | TC-0018 | Automated package-query test | Planned | Not yet produced |
| REQ-0018.007 | TC-0018 | Automated ambiguity test | Planned | Not yet produced |
| REQ-0018.008 | TC-0018 | Schema and redirected-output test | Planned | Not yet produced |
| REQ-0018.009 | TC-0018 | Automated parity and redaction inspection | Planned | Not yet produced |
| REQ-0019.001 | TC-0019 | Fault injection and record inspection | Planned | Not yet produced |
| REQ-0019.002 | TC-0019 | Automated tamper/no-mutation test | Planned | Not yet produced |
| REQ-0019.003 | TC-0019 | Automated state-transition and trust test | Planned | Not yet produced |
| REQ-0019.004 | TC-0019 | Automated classification/idempotence test | Planned | Not yet produced |
| REQ-0019.005 | TC-0019 | Automated deletion-boundary and race test | Planned | Not yet produced |
| REQ-0019.006 | TC-0019 | Output and documentation inspection | Planned | Not yet produced |
| REQ-0019.007 | TC-0019 | Documentation inspection and restore demonstration | Planned | Not yet produced |
| REQ-0019.008 | TC-0019 | Architecture-matrix test and evidence inspection | Planned | Not yet produced |
| REQ-0019.009 | TC-0019 | Failure-message inspection | Planned | Not yet produced |
| REQ-0020.001 | TC-0020 | Harness isolation self-test | Planned | Not yet produced |
| REQ-0020.002 | TC-0020 | Execution-record schema inspection | Planned | Not yet produced |
| REQ-0020.003 | TC-0020 | Corpus metadata validation | Planned | Not yet produced |
| REQ-0020.004 | TC-0020 | Resource-bound and workflow inspection | Planned | Not yet produced |
| REQ-0020.005 | TC-0020 | Controlled failure/rerun demonstration | Planned | Not yet produced |
| REQ-0020.006 | TC-0020 | Synthetic release-gate test | Planned | Not yet produced |
| REQ-0020.007 | TC-0020 | Matrix metadata and environment inspection | Planned | Not yet produced |
| REQ-0021.001 | TC-0021 | Strict C99 compilation | Planned | Not yet produced |
| REQ-0021.002 | TC-0021 | Static platform-boundary inspection | Planned | Not yet produced |
| REQ-0021.003 | TC-0021 | CI configuration and negative compile test | Planned | Not yet produced |
| REQ-0021.004 | TC-0021 | Documentation coverage validation | Planned | Not yet produced |
| REQ-0021.005 | TC-0021 | Security/interface documentation inspection | Planned | Not yet produced |
| REQ-0021.006 | TC-0021 | Warning-as-error reference generation | Planned | Not yet produced |
| REQ-0021.007 | TC-0021 | Supported-platform regression matrix | Planned | Not yet produced |
| REQ-0022.001 | TC-0022 | Migration-guide inspection | Planned | Not yet produced |
| REQ-0022.002 | TC-0022 | Release-note/example validation | Planned | Not yet produced |
| REQ-0022.003 | TC-0022 | Documentation and support review | Planned | Not yet produced |
| REQ-0022.004 | TC-0022 | Prior-stable architecture-matrix journey | Planned | Not yet produced |
| REQ-0022.005 | TC-0022 | Native execution evidence inspection | Planned | Not yet produced |
| REQ-0022.006 | TC-0022 | Release-artifact and gate verification | Planned | Not yet produced |
| REQ-0022.007 | TC-0022 | Release Readiness record review | Planned | Not yet produced |
| REQ-0022.008 | TC-0022 | Release Record and evidence review | Planned | Not yet produced |
| REQ-0022.009 | TC-0022 | Retrospective/improvement record review | Planned | Not yet produced |
| REQ-0023.001 | TC-0023 | Automated static test and inspection | Planned | CI evidence not yet produced |
| REQ-0023.002 | TC-0023 | Automated ADR structure/reference test | Planned | CI evidence not yet produced |
| REQ-0023.003 | TC-0023 | Automated DFS coverage test | Planned | CI evidence not yet produced |
| REQ-0023.004 | TC-0023 | Automated bidirectional reference test | Planned | CI evidence not yet produced |
| REQ-0023.005 | TC-0023 | Automated LF/CRLF equivalence and negative fixture test | Planned | CI evidence not yet produced |
| REQ-0023.006 | TC-0023 | Automated planned-test baseline, completion-status, and negative fixture test | Planned | CI evidence not yet produced |

TC-0014 through TC-0022 now have controlled specifications and executable
runner contracts. Each runner deterministically describes Fast,
PlatformMatrix, Quality, ManualRealEnvironment, and ReleaseGate allocation and
returns Blocked rather than producing pass evidence while its product
implementation remains Proposed. Those runners are not registered as product
CTest cases until their executable assertions and requirements are Accepted.
All rows therefore remain Planned and the evidence column honestly records
that execution evidence has not been produced. TC-0023 has an implemented
controlled specification, automated runner, traceability-validator coverage,
and CTest allocation, but its rows remain Planned until retained CI evidence
exists for the controlled merge baseline.
