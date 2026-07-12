# REQ-0007: WPM Self-Installation

## Description

WPM shall provide package-maintainer deployment scripts that install and
remove the WPM executable independently of WPM's package extraction location.

`setup.cmd` shall:

- install the executable supplied as its first argument, or `wpm.exe` beside
  the script when no argument is supplied;
- install the executable to `%ProgramFiles%\WPM\wpm.exe` by default;
- create the installation directory when needed; and
- create `%ProgramData%\WPM` as the default mutable-data directory; and
- terminate with an exit code of `0` after a successful copy.

`remove.cmd` shall remove both the WPM installation directory and its mutable
data directory, and terminate with an exit code of `0` when removal succeeds
or the directories are already absent.

For automated verification, both scripts shall use `WPM_INSTALL_DIR` as an
explicit override of the default installation directory and `WPM_DATA_DIR` as
an explicit override of the default mutable-data directory.

## Rationale

ADR-0003 assigns installation and removal behavior to package maintainers and
does not prescribe a universal deployment location. These scripts define the
deployment behavior for the WPM package itself while retaining a predictable
default location for users.

## Verification

Verified by:

- TC-0007 - WPM self-installation
