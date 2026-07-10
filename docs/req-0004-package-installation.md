# REQ-0004 - Package Installation

## Description

The `wpm install` command shall install a ZIP package archive.

When invoked with a valid package archive, the application shall:

- derive the installation directory from the archive name,
- extract archive contents under the installation root,
- preserve nested archive paths, and
- terminate normally with an exit code of `0`.

## Rationale

Users need package archives to be expanded into a predictable installation
location without losing package directory structure.

## Verification

Verified by:

- TC-0004 - Package archive installation
