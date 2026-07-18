# REQ-0001: Command-Line Invocation

## Description

The `wpm` executable shall execute from the command line without requiring
additional configuration.

When invoked with no command-line arguments, the application shall:

- initialize successfully,
- display version or usage information,
- terminate normally, and
- return an exit code of `0`.

When invoked with the `--version` option, the application shall:

- display the WPM version and the version and commit of each bundled dependency,
- terminate normally, and
- return an exit code of `0`.

An exact Git tag shall be displayed unchanged as the WPM version. A development
build shall display `<last-tag>-dev+<commits-since-tag>.<short-commit>`, with
`.dirty` appended when tracked files have uncommitted changes. CI checkouts
shall include complete main-repository tag history so development versions use
the last reachable release tag rather than the no-tag `0.0.0` fallback.
CI shall resolve tagged bundled-dependency versions once and provide the same
version and commit metadata to every architecture build.

The `--verbose` option shall be accepted before or after a command. It shall
retain the command's normal output and add detailed progress for file-specific
operations, including package indexing, hashing, archive creation, extraction,
verification, and package script execution.

When verbose output is requested, the application shall report its runtime
mode according to REQ-0010.

## Rationale

Users must be able to verify that the application is installed and
operational before attempting package management operations.

## Verification

Verified by:

- TC-0001 - Usage/version check
- TC-0003 - Package archive build
- TC-0004 - Package archive installation
