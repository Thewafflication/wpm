# WPM WSP Adoption Record

**Content type:** Controlled project record

**Project:** Waughtal Package Manager (WPM)

**WSP baseline:** Immutable commit
`44f591d6416d18252596d8c9fb45f8fbaa65d08a`

**Submodule path:** `wsp/`

**Pinned commit:** `44f591d6416d18252596d8c9fb45f8fbaa65d08a`

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

The requirement-by-requirement disposition matrix is the next controlled WSP
integration artifact. Until that matrix is reviewed, this proposed record does
not claim WSP conformance or release-baseline approval.

| WSP requirement set | Initial disposition | Project artifact |
| --- | --- | --- |
| `WSP-ROB-####` | Review required | Input-validation implementation and tests |
| `WSP-REQM-####` | Review required | Requirements and traceability documents |
| `WSP-DOC-####` | Review required | Documentation sources and release workflow |
| `WSP-PROC-####` | Review required | Project and release process |
| `WSP-TEST-####` | Review required | Test strategy, cases, automation, and evidence |
| `WSP-SEC-####` | Review required | `docs/dfs.md` and security ADRs |
| `WSP-CSTYLE-####` | Review required | C sources and lint automation |
| `WSP-WINRES-####` | Review required | Generated Windows version resources |
| `WSP-SIGN-####` | Review required | Release signing and Defender controls |
| `WSP-TOOL-####` | Review required | Project use of pinned WSP tools |

Permitted final dispositions are Applicable, Tailored, Not applicable, and
Deferred. Every requirement in the common baseline and selected profiles shall
receive an individual disposition before this record becomes Approved.

## Tailoring Decisions

No requirement-level tailoring decisions have been approved. The Personal
Process profile is not selected for the initial WPM baseline because its
individual measurement practices are outside the scope of project-level WSP
integration.

## Baseline History

| Date | WSP baseline | Project change | Summary |
| --- | --- | --- | --- |
| 2026-07-28 | `44f591d6416d18252596d8c9fb45f8fbaa65d08a` | Initial integration | Proposed adoption and profile selection |

The current baseline, pinned commit, and `wsp` gitlink shall agree. An upgrade
entry shall reference the adopting-project change that reviewed the new WSP
requirements and tailoring impact.
