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

`docs/traceability-1.0.md` defines the accepted 1.x release-baseline
allocation. `docs/traceability-2.0.md` defines subordinate-obligation
allocation for the proposed 2.0 baseline and distinguishes Planned from
Verified evidence. Proposed requirements may retain Planned allocation while
their implementation slices are incomplete; they do not satisfy a release
claim.

`tests/verify-traceability.ps1` rejects malformed or duplicate identifiers,
unidentified normative obligations, missing back-references or 2.0 allocation,
incomplete accepted test specifications, absent accepted runners, and
unregistered accepted CTest cases. The `-ReleaseBaseline 2.0` gate additionally
rejects Proposed requirements, Planned rows, missing controlled test artifacts,
and missing objective evidence. `tests/verify-traceability-validator.ps1`
provides positive and negative tests of these rules.

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

Proposed 2.0 behavior may have a controlled specification and runner contract
before implementation. Such a runner shall expose a deterministic
`-Describe` plan using `wpm.test-plan.v1` and shall return `Blocked`, not Pass,
when asked to execute an unimplemented profile. It shall not generate release
evidence. Proposed runtime tests are not registered as ordinary passing CTest
cases until their implementation and executable assertions are accepted.

The normal traceability gate requires exactly one controlled specification and
runner contract for each Proposed or Accepted 2.0 requirement. It validates
the requirement allocation, execution profiles, objective expected results,
evidence paths, and gates exposed by each runner. The release-baseline gate
continues to require Accepted requirements, implemented CTest registration,
Verified rows, and retained objective evidence.

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

### 8.1 WPM 2.0 execution profiles

Every TC-0014 through TC-0022 plan defines all five profiles, states whether
the profile is required, and gives a rationale. A profile marked supporting or
not applicable does not replace another required profile.

| Profile | Purpose | Normal trigger | Gate |
| --- | --- | --- | --- |
| Fast | Deterministic isolated tests, schema/static checks, and bounded fixtures | Pull request and branch CI | `pull-request` |
| PlatformMatrix | Native Debug verification and Release smoke coverage on x86, x64, and ARM64 | CI and release candidate | `2.0-platform-matrix` |
| Quality | Bounded endurance, fuzz, malformed-input, fault, race, and resource cases | Manual, nightly, and prerelease | `quality-program` |
| ManualRealEnvironment | Controlled console, SMB, removable/optical media, protected-key, administrator, usability, or approval evidence that cannot be simulated reliably | Managed environment or accountable review | `environmental-evidence` |
| ReleaseGate | Aggregation and inspection of exact required evidence without rerunning or reclassifying it | Release candidate | `2.0-release-readiness` |

Fast tests use test-only keys and disposable roots and target completion within
the pull-request budget. Environmental tests identify the operator, physical
or managed environment, exact procedure, observation, limitations, and
approval. Platform claims distinguish native, approved emulated, and build-
only evidence; build-only evidence cannot satisfy an architecture test.

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

For WPM 2.0, the feature gates are cumulative: TC-0014 presentation and help;
TC-0015 plans, confirmation, and dry-run purity; TC-0016 transport and managed-
environment media/network coverage; TC-0017 authoring/sign/copy/consume;
TC-0018 read-only queries and `wpm.output.v1`; TC-0019 recovery, destructive
cleanup, restore, failure retention, and native architectures; TC-0020 quality
completion; TC-0021 strict C99, reference documentation, and native regression;
and TC-0022 exact release experience/readiness. A missing required profile or
environmental record blocks its feature gate and therefore blocks TC-0022.

## 12. Evidence and reporting

Each execution record shall identify the test and requirement revisions,
software source revision and version, architecture, configuration, operating
system, toolchain and dependencies, start and finish times, executed command,
exit status, controlled test status, and diagnostic locations.

Generated reports summarize coverage, status, environment, unresolved
deviations, and links or paths to supporting evidence. Report generation shall
fail on missing, malformed, duplicated, or inconsistent required inputs.

Planned 2.0 evidence uses the controlled convention
`Testing/Evidence/2.0/<source-revision>/<TC>/<profile>/<architecture>/`.
An execution record and report, diagnostics, before/after snapshots, fixtures
or corpus identity, and manual/environmental record are stored beneath that
root as applicable. CI may publish the same relative hierarchy inside an
immutable workflow artifact. Placeholder directories and runner descriptions
are not execution evidence.

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
- Traceability: `docs/traceability-1.0.md` and
  `docs/traceability-2.0.md`
- Security design: `docs/dfs.md`
- Verification ADR: `docs/adr-0007-automated-test-strategy-and-verification-artifacts.md`
- WSP test baseline: `wsp/testing/test-strategy.md`
