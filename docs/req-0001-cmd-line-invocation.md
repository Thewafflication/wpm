# REQ-0001 - Command-Line Invocation

## Description

The `wpm` executable shall execute from the command line without requiring
additional configuration.

When invoked with no command-line arguments, the application shall:

- initialize successfully,
- display version or usage information,
- terminate normally, and
- return an exit code of `0`.

When invoked with the `--version` option, the application shall:

- display version information,
- terminate normally, and
- return an exit code of `0`.

## Rationale

Users must be able to verify that the application is installed and
operational before attempting package management operations.

## Verification

Verified by:

- TC-0001 - Usage/version check
