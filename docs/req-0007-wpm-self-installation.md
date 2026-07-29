# REQ-0007: WPM Self-Installation

**Content type:** Project requirements

**Status:** Accepted

**Source:** WPM bootstrap and maintenance workflow

## Scope

Applies to installation, upgrade, diagnosis, and removal of WPM itself.

## Requirement

**REQ-0007.001**
WPM shall provide package-maintainer deployment scripts that install and
remove the WPM executable independently of WPM's package extraction location.

**REQ-0007.002**
The published `install.cmd` bootstrapper shall require Windows PowerShell,
download `index.json` from the stable GitHub latest-release asset URL, select
exactly one `wpm` package matching the native x86, x64, or ARM64 Windows
architecture, and reject unsupported index schemas or unsafe asset names. It
shall download the selected archive and `wpm-release.public` over HTTPS into a
unique temporary directory. Before setup, it shall use the extracted WPM
executable and an isolated temporary data root to trust the downloaded public
key and run `wpm verify` against the archive. It shall then invoke the
packaged `setup.cmd`, propagate failure, and remove temporary content after
success or failure. The bootstrap process's initial trust anchor is GitHub
HTTPS.

**REQ-0007.003**
The bootstrapper shall report concise default progress while checking the
latest release, downloading, extracting, validating, and installing WPM.

**REQ-0007.004**
`setup.cmd` shall:

- install the executable supplied as its first argument, or `wpm.exe` beside
  the script when no argument is supplied;
- preserve compatibility with legacy packages that include an optional
  `wcrt.dll` sidecar, while official Release executables remain self-contained;
- install the executable to the native architecture's Program Files directory
  (using `%ProgramW6432%` when available) under `WPM\wpm.exe` by default;
- install `README.md`, `LICENSE.txt`, and `THIRD_PARTY_NOTICES.md` beside the
  executable, and install the packaged Markdown documentation under `WPM\docs`;
- create the installation directory when needed; and
- create the machine-level `WPM` environment variable with the installation
  directory as its value; and
- add `%WPM%` to the machine-level `Path` when it is not already present; and
- terminate with an exit code of `0` after a successful copy.

**REQ-0007.005**
`setup.cmd --user` shall provide a non-elevated alternative by installing to
`%LocalAppData%\WPM` by default, using the `HKCU\Environment` environment
entries, and setting `WPM_DATA_DIR` to `%LocalAppData%\WPM\data`. It shall add
the user `WPM` variable to the user `Path` only once.

**REQ-0007.006**
When no scope is specified, `setup.cmd` shall detect whether it has an elevated
Windows token and select machine scope when elevated or user scope otherwise.

**REQ-0007.007**
`remove.cmd` shall remove both the WPM installation directory and its mutable
data directory, remove the `WPM` machine environment variable and its `%WPM%`
machine `Path` entry, and terminate with an exit code of `0` when removal
succeeds or the directories are already absent.

**REQ-0007.008**
`remove.cmd --user` shall remove the user-scoped installation, data directory,
and the user `WPM`, `WPM_DATA_DIR`, and `%WPM%` `Path` entries.
When no scope is specified, it shall use the same elevation-based scope
selection as `setup.cmd`.

**REQ-0007.009**
For automated verification, both scripts shall use `WPM_INSTALL_DIR` as an
explicit override of the default installation directory and `WPM_DATA_DIR` as
an explicit override of the default mutable-data directory. They shall also
use `WPM_ENVIRONMENT_REGISTRY_KEY` as an override of the machine environment
registry key for automated verification.

## Rationale

ADR-0003 assigns installation and removal behavior to package maintainers and
does not prescribe a universal deployment location. These scripts define the
deployment behavior for the WPM package itself while retaining a predictable
default location for users.

## Verification

**Method:** Automated test and inspection

**References:** TC-0007 and `tests/tc-0007-*.ps1`

Verified by:

- TC-0007 - WPM self-installation

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`setup.cmd`, `remove.cmd`, `wpm/main.c`, and `tests/tc-0007-wpm-self-installation.ps1` currently implement and verify this requirement.
