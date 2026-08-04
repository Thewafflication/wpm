# WPM 2.0 Requirements Traceability

**Content type:** Controlled traceability matrix

**Status:** Proposed

**Baseline:** Proposed WPM 2.0 requirements REQ-0014 through REQ-0022

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
| REQ-0014.001 | TC-0014 | Automated test and inspection | Planned | Not yet produced |
| REQ-0014.002 | TC-0014 | Automated test and inspection | Planned | Not yet produced |
| REQ-0014.003 | TC-0014 | Automated test and demonstration | Planned | Not yet produced |
| REQ-0014.004 | TC-0014 | Automated test | Planned | Not yet produced |
| REQ-0014.005 | TC-0014 | Automated test and secret-redaction inspection | Planned | Not yet produced |
| REQ-0014.006 | TC-0014 | Automated test and inspection | Planned | Not yet produced |
| REQ-0014.007 | TC-0014 | Automated negative test | Planned | Not yet produced |
| REQ-0014.008 | TC-0014 | Automated redirected-output test | Planned | Not yet produced |
| REQ-0015.001 | TC-0015 | Automated test and inspection | Planned | Not yet produced |
| REQ-0015.002 | TC-0015 | Automated lifecycle test | Planned | Not yet produced |
| REQ-0015.003 | TC-0015 | Automated input-partition test | Planned | Not yet produced |
| REQ-0015.004 | TC-0015 | Automated state-difference test | Planned | Not yet produced |
| REQ-0015.005 | TC-0015 | Automated negative test | Planned | Not yet produced |
| REQ-0015.006 | TC-0015 | Automated state-transition test | Planned | Not yet produced |
| REQ-0016.001 | TC-0016 | Integration test and inspection | Planned | Not yet produced |
| REQ-0016.002 | TC-0016 | Automated filesystem/media test | Planned | Not yet produced |
| REQ-0016.003 | TC-0016 | Automated test and managed SMB demonstration | Planned | Not yet produced |
| REQ-0016.004 | TC-0016 | Automated policy and warning test | Planned | Not yet produced |
| REQ-0016.005 | TC-0016 | Automated signature/trust negative test | Planned | Not yet produced |
| REQ-0016.006 | TC-0016 | Automated output/audit inspection | Planned | Not yet produced |
| REQ-0016.007 | TC-0016 | Fault injection and media demonstration | Planned | Not yet produced |
| REQ-0016.008 | TC-0016 | Automated input-partition test | Planned | Not yet produced |
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

TC-0014 through TC-0022 are planned identifiers, not completed test
specifications or executions. Their controlled specifications, automated
runners, non-test verification procedures, and CTest allocation are created
with implementation slices before a requirement changes from Proposed to
Accepted or a matrix row changes to Verified.
