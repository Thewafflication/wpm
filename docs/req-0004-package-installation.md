# REQ-0004: Package Installation

## Description

The `wpm install` command shall install a ZIP package archive.

When invoked with a valid package archive, the application shall:

- extract archive contents to `%ProgramData%\WPM\temp\<archive-name>`, where
  `<archive-name>` is the ZIP file name without its `.zip` extension,
- preserve nested archive paths during extraction,
- verify the extracted package index before executing package installation
  logic,
- execute `.wpm\install.cmd`, when present, with the staging directory as its
  working directory,
- fail the installation when extraction, index verification, or
  `install.cmd` fails,
- copy the successfully installed ZIP archive to
  `%ProgramData%\WPM\packages\<archive-name>.zip`, and
- remove the staging directory after the installation succeeds or fails.

The package store shall retain the archive using its original archive file name
at `%ProgramData%\WPM\packages\<archive-name>.zip`. The stored archive is the
local record used by future package-management operations; it is not an
extraction destination or a software deployment location.

`install.cmd` is responsible for deploying software from the staging directory
to its required location. WPM shall not impose a deployment location on the
package.

When invoked with `--verbose`, the command shall additionally report archive
extraction, per-file index verification and hashing, script execution, and
archive storage operations.

## Rationale

Users need a predictable, verified installation process, while package authors
need freedom to deploy software to the locations their packages require. A
local archive copy supports auditing, removal, and future upgrade operations
without treating the staging directory as an installed package location.

## Verification

Verified by:

- TC-0004 - Package archive installation
