# WPM WSP Adoption Record

**Content type:** Controlled project record

**Project:** Waughtal Package Manager (WPM)

**WSP baseline:** Immutable commit
`2198ccab08f969a789448767fe7017b774369adc`

**Submodule path:** `wsp/`

**Pinned commit:** `2198ccab08f969a789448767fe7017b774369adc`

**Status:** Proposed

**Approval:** Initial WSP integration change

## Common Baseline

| Requirement set or practice | Applicability | Project artifact or scope |
| --- | --- | --- |
| Common requirements management | Yes | `docs/req-*.md` and `docs/traceability-1.0.md` |
| WSP software lifecycle | Yes | Repository change and release process |
| Project process | Yes | Planning through support and improvement |
| Documentation requirements | Yes | Controlled files under `docs/` |
| Documentation style and identifiers | Yes | Project-authored artifacts |
| Testing requirements | Yes | `docs/ts-0001-test-strategy.md`, test cases, and `tests/` |

Common requirements may be tailored only through an approved requirement
disposition. They are not removed by omitting a selectable profile.

## Selected Profiles

| Profile | Selected | Project scope or rationale |
| --- | --- | --- |
| Personal process | No | Not selected for the initial project baseline |
| Security/DFS | Yes | Package trust, repository trust, and secure update design |
| C source style | Yes | Project-owned C source under `wpm/` |
| PowerShell style | Yes | Project-owned build and test automation |
| CMake style | Yes | Project-owned build configuration |
| Windows version resources | Yes | Shipped `wpm.exe` artifacts |
| Windows code signing and Defender | Yes | Shipped Windows release artifacts |
| Common tools | Yes | WSP validation, reporting, and documentation tools |

## Requirement Dispositions

This matrix records every requirement in the common baseline and selected
profiles. Applicable means current project evidence has been identified;
Deferred means the obligation is selected but its implementation or evidence is
incomplete. This proposed record does not claim WSP conformance or release-
baseline approval while any required disposition remains Deferred.

| WSP requirement | Disposition | Project artifact or completion note |
| --- | --- | --- |
| `WSP-ROB-0001` | Applicable | Input validation in `wpm/`; negative coverage in TC-0002 through TC-0013 |
| `WSP-REQM-0001` | Applicable | Stable parent and subordinate IDs in `docs/req-*.md`, including proposed REQ-0014 through REQ-0022 |
| `WSP-REQM-0002` | Applicable | Controlled requirement structure and reviewable 1.x and proposed 2.0 obligations |
| `WSP-REQM-0003` | Applicable | Scope, source, rationale, relationships, change impact, and implementation allocation records |
| `WSP-REQM-0004` | Applicable | Verification method and TC reference in every requirement document; subordinate allocation in both traceability matrices |
| `WSP-REQM-0005` | Applicable | `docs/traceability-1.0.md`, `docs/traceability-2.0.md`, and positive/negative validator coverage |
| `WSP-REQM-0006` | Applicable | This adoption record and pinned `wsp/` gitlink |
| `WSP-REQM-0007` | Applicable | This matrix and tailoring/deferment register |
| `WSP-REQM-0008` | Applicable | Project process and controlled change-impact template |
| `WSP-REQM-0009` | Applicable | Git history preserves requirement changes and identifiers |
| `WSP-REQM-0010` | Deferred | Add an exact requirements baseline to each release record |
| `WSP-DOC-0001` | Applicable | Documentation workflow builds and publishes one controlled release PDF |
| `WSP-DOC-0002` | Applicable | `documentation/documentation-manifest.json` and WSP negative tests |
| `WSP-DOC-0003` | Applicable | Pinned WSP builder generates project, revision, date, and tool identity |
| `WSP-DOC-0004` | Applicable | `tests/verify-release-documentation.py` verifies contents, outline, and links |
| `WSP-DOC-0005` | Applicable | Shared WSP presentation is applied and the rendered PDF is reviewed |
| `WSP-DOC-0006` | Applicable | Controlled Markdown and manifest are the release-document authority |
| `WSP-DOC-0007` | Applicable | Outputs are isolated under project-owned `output/pdf/` and `tmp/pdfs/` |
| `WSP-DOC-0008` | Applicable | Pinned WSP tests exercise documentation failure paths in CI |
| `WSP-DOC-0009` | Applicable | CI verifies structure and renders every page for release review |
| `WSP-DOC-0010` | Applicable | Verifier compares required metadata, version, language, and structure |
| `WSP-DOC-0011` | Applicable | Workflow publishes an exact single-line `SHA256SUMS` entry |
| `WSP-DOC-0012` | Applicable | GitHub attestation is generated and verified against repository identity |
| `WSP-DOC-0013` | Not applicable | WPM has no contractual or release requirement for PAdES signing |
| `WSP-PROC-0001` | Applicable | `docs/project-process.md` defines the WPM lifecycle and records |
| `WSP-PROC-0002` | Applicable | Project-process role and approval table |
| `WSP-PROC-0003` | Applicable | Proportional planning rules and Work Plan template |
| `WSP-PROC-0004` | Applicable | Change-impact procedure, controlled template, and `docs/change-impact-2.0-requirements-baseline.md` |
| `WSP-PROC-0005` | Applicable | Review criteria, finding rules, approval, and Review Record template |
| `WSP-PROC-0006` | Applicable | Defect/security fields, states, handling, and record templates |
| `WSP-PROC-0007` | Deferred | Add a completed release-readiness record |
| `WSP-PROC-0008` | Deferred | Add release approval and exact baseline records |
| `WSP-PROC-0009` | Applicable | `docs/support-policy.md`, `SECURITY.md`, and DFS response process |
| `WSP-PROC-0010` | Deferred | Define release/incident retrospectives and improvement records |
| `WSP-TEST-0001` | Applicable | Requirement/TC allocation and automated traceability validation |
| `WSP-TEST-0002` | Applicable | Version-controlled `docs/tc-*.tex` specifications |
| `WSP-TEST-0003` | Applicable | Pinned WSP test-case fields enforced by traceability validation |
| `WSP-TEST-0004` | Applicable | Test specifications feed generated reports and evidence |
| `WSP-TEST-0005` | Applicable | Isolated runners establish and clean controlled state |
| `WSP-TEST-0006` | Applicable | TC-0001 through TC-0013 have automated PowerShell runners |
| `WSP-TEST-0007` | Applicable | Generated execution evidence records baseline and environment metadata |
| `WSP-TEST-0008` | Applicable | Controlled statuses in WSP library and evidence validation |
| `WSP-TEST-0009` | Deferred | Enforce durable preservation and linkage of failed runs and reruns |
| `WSP-TEST-0010` | Applicable | CMake and PowerShell generate reports from controlled inputs |
| `WSP-TEST-0011` | Applicable | Traceability and release-baseline validation plus positive/negative validator tests run in CI |
| `WSP-TEST-0012` | Applicable | Branch, pull-request, and release workflows gate required tests |
| `WSP-TEST-0013` | Applicable | x86, x64, and ARM64 release matrix in strategy and workflows |
| `WSP-TEST-0014` | Applicable | Explicit 90-day workflow retention and release attachment policy |
| `WSP-TEST-0015` | Applicable | Every TC identifies its selected test-design technique |
| `WSP-TEST-0016` | Deferred | To be implemented |
| `WSP-TEST-0017` | Applicable | Using a arm64 runner to test arm64 build |
| `WSP-TEST-0018` | Deferred | To be implemented |
| `WSP-SEC-0001` | Applicable | DFS scope, assets, assumptions, consequences, and non-goals |
| `WSP-SEC-0002` | Applicable | Controlled `docs/dfs.md` with review triggers and traceability |
| `WSP-SEC-0003` | Applicable | DFS trust actors, inputs, boundaries, entry points, and operations |
| `WSP-SEC-0004` | Applicable | WPM-THR-001 through WPM-THR-012 control matrix |
| `WSP-SEC-0005` | Applicable | DFS goals map to identified REQ and TC artifacts |
| `WSP-SEC-0006` | Applicable | Bounded parsing, staging containment, negative and fault tests |
| `WSP-SEC-0007` | Applicable | Administrative authorization and trust-boundary decisions |
| `WSP-SEC-0008` | Applicable | Ed25519 design, protected keys, memory clearing, and secret policy |
| `WSP-SEC-0009` | Applicable | Pinned dependencies, matrix build, release gates, and DFS review |
| `WSP-SEC-0010` | Applicable | Installation/upgrade audit design and protected-data constraints |
| `WSP-SEC-0011` | Applicable | Fail-closed validation, failure audit, and recovery limitations |
| `WSP-SEC-0012` | Applicable | Security requirements/threats map to tests, inspection, and analysis |
| `WSP-SEC-0013` | Applicable | Security review process and controlled private finding record |
| `WSP-SEC-0014` | Applicable | `SECURITY.md`, support policy, and DFS vulnerability response |
| `WSP-CSTYLE-0001` | Applicable | Doxygen `@file` comments cover all 21 project-owned C/header files |
| `WSP-CSTYLE-0002` | Deferred | Document every function contract or approved declaration reference |
| `WSP-CSTYLE-0003` | Deferred | Document public and non-obvious internal entities |
| `WSP-CSTYLE-0004` | Deferred | Eliminate or individually tailor 415 lines over 80 characters |
| `WSP-CSTYLE-0005` | Deferred | Add complete line-length and Doxygen warnings-as-errors gates |
| `WSP-WINRES-0001` | Applicable | CMake generates and links `VERSIONINFO` for `wpm.exe` |
| `WSP-WINRES-0002` | Applicable | `cmake/GenerateVersion.cmake` owns generated resource source |
| `WSP-WINRES-0003` | Applicable | Numeric four-part version generation uses bounded components |
| `WSP-WINRES-0004` | Applicable | File/product versions derive from the controlled project version |
| `WSP-WINRES-0005` | Deferred | Complete and verify every required version string field |
| `WSP-WINRES-0006` | Applicable | Internal and original filenames identify `wpm` and `wpm.exe` |
| `WSP-WINRES-0007` | Applicable | Resource declares Windows NT and application file type |
| `WSP-WINRES-0008` | Applicable | Generated resource defines mask and release file flags |
| `WSP-WINRES-0009` | Applicable | Resource uses controlled US-English/Unicode translation |
| `WSP-WINRES-0010` | Deferred | Add x86/x64/ARM64 resource-to-artifact consistency checks |
| `WSP-WINRES-0011` | Deferred | Add required publisher, legal, and public-information fields |
| `WSP-WINRES-0012` | Deferred | Add automated final-artifact resource verification |
| `WSP-SIGN-0001` | Deferred | Approve an Authenticode and certificate acquisition plan |
| `WSP-SIGN-0002` | Deferred | Document separation of package, Authenticode, and provenance trust |
| `WSP-SIGN-0003` | Deferred | Provision and protect an authorized signing identity |
| `WSP-SIGN-0004` | Deferred | Add SHA-256 Authenticode signing for release executables |
| `WSP-SIGN-0005` | Deferred | Add a trusted RFC 3161 timestamp service |
| `WSP-SIGN-0006` | Deferred | Record whether any legacy-signing exception is required |
| `WSP-SIGN-0007` | Deferred | Enforce signing order and post-signing immutability |
| `WSP-SIGN-0008` | Deferred | Gate release on independent signature verification |
| `WSP-SIGN-0009` | Deferred | Bind signature evidence to exact artifact digest and identity |
| `WSP-SIGN-0010` | Deferred | Run Microsoft Defender against final release artifacts |
| `WSP-SIGN-0011` | Deferred | Fail release on unresolved malware detection |
| `WSP-SIGN-0012` | Deferred | Classify every detection and retain evidence |
| `WSP-SIGN-0013` | Deferred | Define false-positive investigation and approval |
| `WSP-SIGN-0014` | Deferred | Define Microsoft submission and tracking when required |
| `WSP-SIGN-0015` | Deferred | Prohibit evasion and routine security exclusions in release work |
| `WSP-SIGN-0016` | Deferred | Publish signed-artifact and warning guidance |
| `WSP-SIGN-0017` | Deferred | Define certificate renewal, revocation, and compromise response |
| `WSP-SIGN-0018` | Deferred | Retain complete release trust evidence |
| `WSP-TOOL-0001` | Applicable | CI invokes tools from the pinned `wsp/` gitlink |
| `WSP-TOOL-0002` | Applicable | Build supplies explicit project and output locations |
| `WSP-TOOL-0003` | Applicable | Self-tests and repeated local execution verify deterministic results |
| `WSP-TOOL-0004` | Applicable | WSP suite covers positive, negative, and missing-input contracts |
| `WSP-TOOL-0005` | Applicable | Negative tests inspect rule- and artifact-specific diagnostics |
| `WSP-TOOL-0006` | Applicable | Generated output remains in project-owned locations |
| `WSP-TOOL-0007` | Applicable | WSP self-tests cover secret-safe evidence behavior |
| `WSP-TOOL-0008` | Applicable | Documentation CI runs pinned WSP tool self-verification |
| `WSP-TOOL-0009` | Applicable | Workflows use current Node 24-compatible action majors |

## Tailoring Decisions

The following register controls every non-Applicable disposition. Requirement
ranges refer to every identifier in the inclusive range; individual completion
is recorded by updating the corresponding matrix row.

- **`WSP-REQM-0010` — Deferred.** Exact release-baseline records are not yet
  produced, so a release claim may omit its complete requirement identity.
  Git history, traceability validation, and release CI compensate. Completion
  requires a release record identifying the exact requirements baseline.
- **`WSP-DOC-0013` — Not applicable.** No stakeholder, contract, or release
  policy requires a PAdES-signed PDF. SHA-256 and provenance attestation are
  the compensating integrity controls; review this decision if such a
  requirement appears.
- **`WSP-PROC-0007`, `0008`, `0010` — Deferred.** Controlled templates exist,
  but no WSP-governed release-readiness, approval, or retrospective record has
  been completed. Existing Git/GitHub history, CI gates, support policy, and
  security policy compensate until those records are reviewed and retained.
- **`WSP-TEST-0009` — Deferred.** CI retains failures, but does not enforce
  failure-to-fix-to-rerun linkage. Ninety-day failed-job artifacts compensate
  until the validator or release process preserves and links that chain.
- **`WSP-CSTYLE-0002`--`0005` — Deferred.** Existing C predates WSP function
  and entity documentation and line-length rules. C99 lint, compiler warnings,
  tests, and review compensate until all in-scope files pass the 80-column and
  Doxygen gates.
- **`WSP-WINRES-0005`, `0010`--`0012` — Deferred.** Generated resources are
  partial and final artifacts lack complete architecture checks. Generated
  version identity and PE compatibility checks compensate until all required
  fields and architectures pass artifact inspection.
- **`WSP-SIGN-0001`--`0018` — Deferred.** Package signatures exist, but the
  Authenticode, timestamp, Defender, certificate, and trust-evidence controls
  do not. Ed25519 package verification and protected release gates compensate
  until the approved certificate plan and all required gates are operational.

The owner for every record is the WPM maintainers, and completion requires an
adoption approval change.

The Personal Process profile is not selected for the initial WPM baseline
because its individual measurement practices are outside project-level WSP
integration. Selecting it later requires dispositions for `WSP-PSP-0001`
through `WSP-PSP-0009`.

## Baseline History

| Date | WSP baseline | Project change | Summary |
| --- | --- | --- | --- |
| 2026-07-28 | `44f591d6416d18252596d8c9fb45f8fbaa65d08a` | Initial integration | Proposed adoption and profile selection |
| 2026-07-28 | `0f66aa65fd820799468818ae57897f2940fb6037` | WPM testing feedback | Added native standard-input testing guidance and WCRT regression expectations |
| 2026-07-31 | `3ed0758083fcc2c439499f251bf245007a8f54a5` |  Added color and defaullt c flags |
| 2026-08-04 | `2198ccab08f969a789448767fe7017b774369adc` | WSP logger portability update | Adopted portable TTY detection, C99 argument copying, and expanded logger tests; no requirement or tailoring dispositions changed |
| 2026-08-04 | `2198ccab08f969a789448767fe7017b774369adc` | WPM 2.0 proposed requirements baseline | Added REQ-0014 through REQ-0022, proposed subordinate traceability, change impact, and validator evidence; no deferred disposition was reported complete |
| 2026-08-04 | `2198ccab08f969a789448767fe7017b774369adc` | WPM 2.0 architecture and DFS baseline | Added ADR-0010 through ADR-0013, DFS threat/control updates, REQ/TC-0023 consistency checks, and scoped 2.0 supersession of HTTPS-only restrictions; no deferred WSP disposition was reported complete |

The current baseline, pinned commit, and `wsp` gitlink shall agree. An upgrade
entry shall reference the adopting-project change that reviewed the new WSP
requirements and tailoring impact.
