# REQ-0003: Package Build

**Content type:** Project requirements

**Status:** Accepted

**Source:** WPM package format and build workflow

## Scope

Applies when WPM builds a package archive from a controlled source directory.

## Requirement

**REQ-0003.001**
The `wpm build` command shall build a ZIP package from a source directory.

**REQ-0003.002**
When invoked with a valid source directory and output directory, the
application shall:

- recursively package source directory contents,
- read package metadata from `.wpm/package.txt`,
- name the archive `<package-name>-<arch>-<version>.zip` for release
  packages,
- name the archive `<package-name>-<arch>-debug-<version>.zip` when package
  metadata sets `debug=true`,
- write the archive to the requested output directory, and
- terminate normally with an exit code of `0`.

**REQ-0003.003**
When invoked with `--verbose`, the command shall additionally report the
metadata, index, hashing, and archive-file operations it performs.

## Rationale

Package authors need a reproducible command for creating distributable package
archives from a source tree.

## Verification

**Method:** Automated test and inspection

**References:** TC-0003 and `tests/tc-0003-*.ps1`

Verified by:

- TC-0003 - Package archive build

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0003-package-archive-build.ps1` currently implement and verify this requirement.
