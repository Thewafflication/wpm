# WPM Quality Testing Program

## Purpose

This program finds reliability, security, and recovery weaknesses that are not
well suited to the fast, deterministic verification suite. It complements CI;
it is not required to run on every commit.

## Test classes

### Stress and endurance

Run repeated install, remove, update, upgrade, repository-index, and
self-upgrade cycles against isolated data roots. Vary package count, archive
size, architecture, retained-version count, and repository availability.

Record duration, failure rate, temporary-file growth, cache growth, handle
leaks where measurable, and whether the final package/audit state is coherent.

### Fuzzing and malformed-input testing

Generate or mutate package archives, package metadata, index JSON, signatures,
repository paths, command-line arguments, and configuration files. Verify WPM
rejects invalid input without crashes, hangs, unsafe path writes, or corrupted
installed state.

Maintain a regression corpus for every discovered failure.

### Fault injection and recovery

Simulate unavailable repositories, removed USB/CD-ROM media, interrupted file
copies, full disks, read-only directories, denied permissions, failed package
scripts, terminated processes, and interrupted self-upgrades. Verify that the
result identifies the failed phase, preserves prior valid state, and offers a
safe retry or cleanup path.

### Transport and media matrix

Exercise HTTPS, explicitly opted-in HTTP, local filesystem, SMB, USB, and
CD-ROM repositories. Cover writable and read-only media, missing media, drive
letter changes, and network-share disconnects.

### Compatibility and performance

Track x86, x64, and ARM64 behavior on supported Windows releases. Run the
separate legacy x86/Windows 2000 investigation described in
[compatibility tracking](compatibility-tracking.md) only when an appropriate
environment is available. Measure large-index and large-package operations to
identify unacceptable regressions.

## Execution model

- Fast deterministic tests remain mandatory in pull-request and release CI.
- Quality tests may run manually, nightly, before release candidates, or in a
  dedicated environment with disposable virtual machines and data roots.
- Long-running, destructive, network-dependent, and fuzz tests must not use
  production repositories, keys, or user data.
- Failures become tracked issues with their command, seed or corpus input,
  environment, logs, and retained artifacts. Reproducible defects gain a
  minimized regression test in the regular suite where practical.

## Initial release-candidate gate

Before a major release candidate, complete and record at least:

- one endurance run covering install/remove/upgrade cycles;
- malformed archive, metadata, index, signature, and path corpus runs;
- self-upgrade interruption and recovery scenarios;
- removable-media and unavailable-repository scenarios; and
- a review of new crashes, hangs, data-loss risks, and security findings.
