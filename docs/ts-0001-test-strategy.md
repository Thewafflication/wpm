# TS-0001: WPM Test Strategy

**Content type:** Controlled test strategy

**Status:** Accepted

**Owner:** WPM maintainers

## 1. Purpose

This strategy defines how WPM plans, implements, executes, and records
verification. It applies the pinned WSP test baseline to project requirements,
supported releases, security controls, and compatibility claims.

## 2. Scope

The strategy covers project-owned source, build logic, scripts, requirements,
test specifications, CI workflows, release artifacts, and generated evidence.
Third-party package payload behavior is excluded except where WPM validates,
stages, invokes, retains, or removes that payload.

## 3. Roles and responsibilities

- **Contributor:** updates affected requirements, tests, implementation, and
  traceability in one controlled change.
- **Reviewer:** confirms requirement quality, test adequacy, risk coverage, and
  the absence of unexplained failed or missing evidence.
- **CI:** builds controlled configurations, validates traceability, executes
  automated tests, and retains reports and diagnostics.
- **Release approver:** confirms that every required matrix entry passed and
  that unresolved deviations are explicitly accepted before publication.

For a single-maintainer change, the same person may perform multiple roles, but
the review and release-gate evidence remain explicit.

## 4. Test levels and verification methods

- **Static verification - inspection and static analysis:** requirement
  structure, traceability, C style, workflows, version resources, and warnings.
- **Component and command - automated test:** individual CLI operations and
  controlled error paths.
- **Integration - automated test:** archives, signing, trust stores,
  repositories, scripts, permissions, and upgrade state.
- **System - automated test and demonstration:** end-to-end package lifecycle
  using the built WPM executable.
- **Release - inspection, test, and analysis:** architecture matrix, signed
  packages, prior-release upgrade, artifact identity, and release evidence.
- **Security - negative test, fault injection, inspection, and analysis:**
  malformed input, denied trust, tampering, traversal, failed scripts, recovery,
  and audit records.

Test specifications identify the selected ISO/IEC/IEEE 29119-4-oriented
technique or explain why a named technique is not applicable.

## 5. Requirements and traceability

Every accepted project requirement and identified subordinate obligation shall
have planned verification. The controlled chain is:

```text
REQ-NNNN and REQ-NNNN.nnn
              |
              v
          TC-NNNN
              |
              v
 tests/tc-NNNN-*.ps1 and CTest
              |
              v
 execution evidence and generated report
```

`docs/traceability-1.0.md` defines release-baseline allocation.
`tests/verify-traceability.ps1` rejects malformed or duplicate identifiers,
unidentified normative obligations, missing back-references, incomplete test
specifications, absent runners, and unregistered CTest cases.

Matching numbers do not establish coverage by themselves. Reviewers shall
confirm that procedures and expected results cover the allocated obligations.

## 6. Controlled test specifications

Each release-evidence test shall have a version-controlled `TC-NNNN` LaTeX
specification under `docs/` and an automated runner under `tests/`. The
specification is the source of truth for purpose, references, preconditions,
environment, assumptions, inputs, initial state, procedure, expected results,
and cleanup.

Test reports shall reference or incorporate the controlled specification and
shall not maintain a separately edited copy of its procedure.

## 7. Environments and provisioning

CI uses architecture-compatible Windows runners. Each job checks out recursive
submodules and provisions the selected TinyCC and WCRT inputs. Tests isolate
mutable state beneath job or test-specific temporary directories and shall not
depend on a developer workstation's WPM configuration or trust store.

Local execution is permitted for development evidence when it records the same
required environment metadata. Local execution does not replace a required CI
or release-matrix result.

## 8. Supported release matrix

- **Architecture:** x86, x64, and ARM64.
- **Operating system:** supported Windows 10 or Windows 11 compatible with the
  architecture; evidence records the exact runner image and OS build.
- **Build configuration:** Debug for complete verification and reports; Release
  for distributable construction and smoke execution.
- **Toolchain:** project-selected TinyCC with the matching WCRT package.
- **Dependency baseline:** pinned Git submodules and recorded external package
  versions.

Every applicable entry shall pass its required gates. Equivalence between
Windows 10 and Windows 11 may be used for architecture-independent behavior
only when documented with residual compatibility risk; release support claims
remain governed by `docs/support-policy.md`.

## 9. Execution and controlled status

Automated runners shall establish preconditions, isolate earlier outputs,
record the command and exit status, and perform documented cleanup. An execution
has exactly one status: Pass, Fail, Blocked, Inconclusive, Not run, or Not
applicable. Any status other than Pass requires a rationale and does not satisfy
a required release gate.

Failure output and the original failing result shall be retained when a test is
rerun. A later pass does not overwrite or reclassify the earlier failure.

## 10. Continuous-integration gates

For each required verification job, CI shall:

1. check out the exact source and recursive submodule revisions;
2. provision recorded tool and dependency versions;
3. validate requirements, test specifications, and bidirectional traceability;
4. build the selected configuration;
5. execute every required test;
6. validate controlled evidence status;
7. generate reports and warning summaries; and
8. upload reports, logs, and failure diagnostics even when a test fails.

A required failing, blocked, inconclusive, or missing result fails the gate.
Emergency acceptance requires a documented deviation, risk decision, owner,
scope, and completion condition.

## 11. Release gates

A tagged release shall not be published until:

- every required Debug verification matrix job passes;
- every Release artifact builds and executes on a compatible runner;
- package signing and verification gates pass;
- upgrade from the previous supported release passes for each architecture;
- traceability and documentation checks pass;
- required reports and diagnostics are retained; and
- unresolved defects or deviations have explicit release approval.

The source revision, WSP pin, dependency baseline, test-specification revision,
and released artifact identities shall be recoverable from the release record.

## 12. Evidence and reporting

Each execution record shall identify the test and requirement revisions,
software source revision and version, architecture, configuration, operating
system, toolchain and dependencies, start and finish times, executed command,
exit status, controlled test status, and diagnostic locations.

Generated reports summarize coverage, status, environment, unresolved
deviations, and links or paths to supporting evidence. Report generation shall
fail on missing, malformed, duplicated, or inconsistent required inputs.

## 13. Evidence retention and integrity

- Pull-request and branch CI evidence is retained for at least 90 days.
- Tagged-release verification evidence is retained as protected workflow
  artifacts for at least 90 days. Published packages, indexes, keys, and
  installer artifacts remain attached for the supported lifetime of the release.
- A failing result used for defect diagnosis is retained through corrective
  verification and closure, even when that exceeds the normal CI period.

Evidence remains associated with exact source, test-specification, dependency,
and artifact revisions. Published checksums, signatures, immutable workflow
artifacts, or release attachments protect release evidence from silent change.

## 14. Exclusions and residual limitations

- WPM does not validate the correctness or safety of arbitrary third-party
  package scripts after the administrator authorizes their execution.
- CI does not claim exhaustive coverage of all Windows patch levels, filesystem
  drivers, antivirus products, proxies, or enterprise policies.
- ARM64 verification depends on availability of an architecture-compatible
  runner and matching toolchain inputs; absence blocks release rather than
  silently substituting emulation.
- Performance and long-running fault-injection work in
  `docs/quality-testing.md` complements, but does not replace, release gates.

## 15. Controlled relationships

- Requirements baseline: `docs/req-*.md`
- Test specifications: `docs/tc-*.tex`
- Traceability: `docs/traceability-1.0.md`
- Security design: `docs/dfs.md`
- Verification ADR: `docs/adr-0007-automated-test-strategy-and-verification-artifacts.md`
- WSP test baseline: `wsp/testing/test-strategy.md`
