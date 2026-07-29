# REQ-0010: Runtime Mode Detection

**Content type:** Project requirements

**Status:** Accepted

**Source:** WPM managed and portable deployment model

## Scope

Applies when WPM selects managed or portable data locations at runtime.

## Requirement

**REQ-0010.001**
WPM shall identify its runtime mode from the executable location.

**REQ-0010.002**
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

**Method:** Automated test and inspection

**References:** TC-0010 and `tests/tc-0010-*.ps1`

Verified by:

- TC-0010 - Runtime mode detection

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0010-runtime-mode-detection.ps1` currently implement and verify this requirement.
