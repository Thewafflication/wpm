# REQ-0002: Package Initialization

**Content type:** Project requirements

**Status:** Accepted

**Source:** WPM package-authoring workflow

## Scope

Applies when a package author initializes a project with `wpm init`.

## Requirement

**REQ-0002.001**
The `wpm init` command shall initialize package metadata in the current
directory.

**REQ-0002.002**
When invoked without an explicit package name, the application shall:

- use the current directory name as the package name,
- create the `.wpm` package metadata directory when needed,
- create missing package support files,
- preserve existing package support files, and
- terminate normally with an exit code of `0`.

**REQ-0002.003**
When invoked with an invalid package name, the application shall:

- reject the package name,
- avoid creating package metadata, and
- terminate with a non-zero exit code.

## Rationale

Package authors need a predictable starting layout that can be safely
regenerated without overwriting edited metadata.

## Verification

**Method:** Automated test and inspection

**References:** TC-0002 and `tests/tc-0002-*.ps1`

Verified by:

- TC-0002 - Package initialization

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0002-package-initialization.ps1` currently implement and verify this requirement.
