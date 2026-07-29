# REQ-0006: Package Metadata Archive Naming

**Content type:** Project requirements

**Status:** Accepted

**Source:** ADR-0003 package format

## Scope

Applies to archive names produced by `wpm build` from package metadata.

## Requirement

**REQ-0006.001**
The `wpm build` command shall name package archives from `.wpm/package.txt`
metadata.

**REQ-0006.002**
When building a package, the application shall:

- read `name`, `version`, `arch`, and `debug` from `.wpm/package.txt`,
- reject missing or unsafe `name`, `version`, or `arch` values while accepting
  `+` in SemVer version build metadata,
- write release packages as `<name>-<arch>-<version>.zip`, and
- write debug packages as `<name>-<arch>-debug-<version>.zip`.

**REQ-0006.003**
The `wpm init` command shall create package metadata with default `version`,
`arch`, and `debug` fields.

## Rationale

Package consumers need archive names that identify the package version,
target architecture, and debug build flavor before installing or publishing
the artifact.

## Verification

**Method:** Automated test and inspection

**References:** TC-0006 and `tests/tc-0006-*.ps1`

Verified by:

- TC-0006 - Package metadata archive naming

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0006-package-metadata-archive-naming.ps1` currently implement and verify this requirement.
