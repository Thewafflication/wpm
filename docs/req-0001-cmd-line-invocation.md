# REQ-0001: Command-Line Invocation

**Content type:** Project requirements

**Status:** Accepted

**Source:** WPM command-line product baseline

## Scope

Applies to the shipped WPM executable on every supported Windows architecture.

## Requirement

**REQ-0001.001**
The `wpm` executable shall execute from the command line without requiring
additional configuration.

**REQ-0001.002**
When invoked with no command-line arguments, the application shall:

- initialize successfully,
- display version or usage information,
- terminate normally, and
- return an exit code of `0`.

**REQ-0001.003**
When invoked with the `--version` option, the application shall:

- display the WPM version and the version and commit of each bundled dependency,
- display the version of each loaded, non-system runtime dependency,
- terminate normally, and
- return an exit code of `0`.

**REQ-0001.004**
An exact Git tag shall be displayed unchanged as the WPM version. A development
build shall display `<last-tag>-dev+<commits-since-tag>.<short-commit>`, with
`.dirty` appended when tracked files have uncommitted changes. CI checkouts
shall include complete main-repository tag history so development versions use
the last reachable release tag rather than the no-tag `0.0.0` fallback.
CI shall resolve tagged bundled-dependency versions once and provide the same
version and commit metadata to every architecture build.

**REQ-0001.005**
The `--verbose` option shall be accepted before or after a command. It shall
retain the command's normal output and add detailed progress for file-specific
operations, including package indexing, hashing, archive creation, extraction,
verification, and package script execution.

**REQ-0001.006**
When verbose output is requested, the application shall report its runtime
mode according to REQ-0010.

## Rationale

Users must be able to verify that the application is installed and
operational before attempting package management operations.

## Verification

**Method:** Automated test and inspection

**References:** TC-0001 and `tests/tc-0001-*.ps1`

Verified by:

- TC-0001 - Usage/version check
- TC-0003 - Package archive build
- TC-0004 - Package archive installation

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0001-usage-version-check.ps1` currently implement and verify this requirement.
