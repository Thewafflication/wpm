# REQ-0003: Package Build

## Description

The `wpm build` command shall build a ZIP package from a source directory.

When invoked with a valid source directory and output directory, the
application shall:

- recursively package source directory contents,
- read package metadata from `.wpm/package.txt`,
- name the archive `<package-name>-<version>-<arch>.zip` for release
  packages,
- name the archive `<package-name>-<version>-<arch>-debug.zip` when package
  metadata sets `debug=true`,
- write the archive to the requested output directory, and
- terminate normally with an exit code of `0`.

## Rationale

Package authors need a reproducible command for creating distributable package
archives from a source tree.

## Verification

Verified by:

- TC-0003 - Package archive build
