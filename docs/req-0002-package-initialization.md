# REQ-0002 - Package Initialization

## Description

The `wpm init` command shall initialize package metadata in the current
directory.

When invoked without an explicit package name, the application shall:

- use the current directory name as the package name,
- create the `.wpm` package metadata directory when needed,
- create missing package support files,
- preserve existing package support files, and
- terminate normally with an exit code of `0`.

When invoked with an invalid package name, the application shall:

- reject the package name,
- avoid creating package metadata, and
- terminate with a non-zero exit code.

## Rationale

Package authors need a predictable starting layout that can be safely
regenerated without overwriting edited metadata.

## Verification

Verified by:

- TC-0002 - Package initialization
