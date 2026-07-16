# REQ-0010: Runtime Mode Detection

## Description

WPM shall identify its runtime mode from the executable location.

- When the executable is located beneath the native architecture's Program
  Files `WPM` directory, WPM shall treat the execution as managed.
- When the executable is located elsewhere, including on removable media, WPM
  shall treat the execution as portable.
- When `--verbose` is specified, WPM shall display the runtime mode. Portable
  output shall also identify the executable path.
- WPM shall create its data directory and `packages`, `temp`, `cache`, and
  `config` subdirectories before performing an operational command. The data
  directory shall be `%ProgramData%\WPM` by default or `WPM_DATA_DIR` when
  explicitly overridden.
- A portable executable directory may be read-only. Operational commands shall
  use the data directory and shall not write mutable state beside the
  executable.
- The `--diagnose` option shall display the runtime mode and resolved
  executable, data, package, cache, and configuration locations without
  initializing data directories.

## Rationale

Users need to know whether WPM is running from a managed installation or a
portable copy before relying on machine-level installation behavior.

## Verification

Verified by:

- TC-0010 - Runtime mode detection
