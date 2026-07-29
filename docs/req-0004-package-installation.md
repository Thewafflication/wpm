# REQ-0004: Package Installation

**Content type:** Project requirements

**Status:** Accepted

**Source:** ADR-0003 package lifecycle

## Scope

Applies to local package-archive installation on supported Windows targets.

## Requirement

**REQ-0004.001**
The `wpm install` command shall install a ZIP package archive.

**REQ-0004.002**
When invoked with a valid package archive, the application shall:

- report the package name with the default progress phases
  `<name>: Extracting package...`, `<name>: Validating package...`, and
  `<name>: Installing package...`, prefixing each phase with `[n of total]`
  when one command processes multiple packages,
- extract archive contents to `%ProgramData%\WPM\temp\<archive-name>`, where
  `<archive-name>` is the ZIP file name without its `.zip` extension,
- preserve nested archive paths during extraction,
- verify the extracted package index before executing package installation
  logic,
- execute `.wpm\install.cmd`, when present, with the staging directory as its
  working directory,
- stream the standard output and standard error from `.wpm\install.cmd` to the
  invoking console, framed by start and completion messages that include the
  script exit code,
- fail the installation when extraction, index verification, or
  `install.cmd` fails,
- copy the successfully installed ZIP archive to
  `%ProgramData%\WPM\packages\<archive-name>.zip`, and
- remove the staging directory after the installation succeeds or fails.

**REQ-0004.003**
The package store shall retain the archive using its original archive file name
at `%ProgramData%\WPM\packages\<archive-name>.zip`. The stored archive is the
local record used by future package-management operations; it is not an
extraction destination or a software deployment location.

**REQ-0004.004**
`install.cmd` is responsible for deploying software from the staging directory
to its required location. WPM shall not impose a deployment location on the
package.

**REQ-0004.005**
When invoked with `--verbose`, the command shall additionally report archive
extraction, per-file index verification and hashing, script execution, and
archive storage operations.

## Rationale

Users need a predictable, verified installation process, while package authors
need freedom to deploy software to the locations their packages require. A
local archive copy supports auditing, removal, and future upgrade operations
without treating the staging directory as an installed package location.

## Verification

**Method:** Automated test and inspection

**References:** TC-0004 and `tests/tc-0004-*.ps1`

Verified by:

- TC-0004 - Package archive installation

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0004-package-archive-installation.ps1` currently implement and verify this requirement.
