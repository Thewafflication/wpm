# REQ-0006: Package Metadata Archive Naming

## Description

The `wpm build` command shall name package archives from `.wpm/package.txt`
metadata.

When building a package, the application shall:

- read `name`, `version`, `arch`, and `debug` from `.wpm/package.txt`,
- reject missing or unsafe `name`, `version`, or `arch` values while accepting
  `+` in SemVer version build metadata,
- write release packages as `<name>-<arch>-<version>.zip`, and
- write debug packages as `<name>-<arch>-debug-<version>.zip`.

The `wpm init` command shall create package metadata with default `version`,
`arch`, and `debug` fields.

## Rationale

Package consumers need archive names that identify the package version,
target architecture, and debug build flavor before installing or publishing
the artifact.

## Verification

Verified by:

- TC-0006 - Package metadata archive naming
