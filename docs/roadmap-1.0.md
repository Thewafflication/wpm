# WPM 1.0 Release Roadmap

## Goal

Deliver a Windows package manager that can build, sign, install, remove, and
upgrade WPM packages from configured repositories with automated, traceable
verification evidence.

## Current Baseline

- Package build, installation, removal, and WPM self-packaging are implemented.
- The generated WPM package identifies its source repository as
  `https://github.com/Thewafflication/wpm`.
- CI runs TC-0001 through TC-0010 and uploads their generated reports and
  execution evidence as a GitHub artifact.
- `wpm upgrade` is a command placeholder; it does not yet resolve or install
  a newer package version.
- Package signing and package-signature validation are not yet implemented.
- Remote package repositories are designed but not yet implemented.
- WPM self-installation creates a machine-level `WPM` variable and adds it to
  the shell path.
- WPM reports managed or portable runtime mode when verbose output is enabled.

## Milestone 1: Installation and Portable Runtime Modes

Make WPM aware of how it is being run while keeping the executable portable
and its mutable state centralized.

Implemented:

- WPM detects a managed installation when the executable is located beneath
  the native architecture's Program Files `WPM` directory.
- WPM treats execution from any other location, including a USB drive, as
  portable and reports this with `--verbose`.
- `wpm --diagnose` displays runtime mode plus resolved executable, data,
  package, cache, and configuration locations.
- The executable, rather than the installer, initializes centralized mutable
  state beneath `%ProgramData%\WPM` for both managed and portable execution.
- Operational commands create `packages`, `temp`, `cache`, and `config`
  beneath the data root without requiring the executable directory to be
  writable.
- `WPM_DATA_DIR` provides an explicit data-root override for test automation
  and advanced deployments.

Remaining work:

- Add managed-installation and read-only portable execution coverage.
- Define durable cache and configuration contents and their lifecycle.
- Decide whether user-scoped data storage is needed for non-elevated scenarios.

Completion criteria:

- Requirements and test cases cover managed, portable, and read-only portable
  execution, plus their expected centralized data locations.
- No command writes mutable state beside a portable executable or into the
  managed executable directory.

## Milestone 2: Command-Line Installation

Make WPM available as a normal command after self-installation. The
machine-wide Path integration is implemented; user-scoped installation and
new-shell command-discovery coverage remain future work.

- Provide a user-scoped installation option when machine-wide elevation is not
  available.

Completion criteria:

- Requirements and test cases cover installation, repeat installation, command
  discovery in a new shell, and removal.
- The installed `wpm.exe` can be invoked by name without its full path.

## Milestone 3: Remote Package Repositories

Enable named sources of packages instead of requiring a local ZIP path.

- Implement repository configuration: add, list, remove, and update.
- Define repository metadata, package indexes, package URLs, and cache layout.
- Support secure HTTPS repositories first; add local, SMB, or SCP providers only
  after the common repository contract is proven.
- Resolve an install request such as `wpm install <package-name>` to a package
  from configured repositories, while retaining `wpm install <package.zip>`.
- Define repository priority, offline behavior, cache expiration, and failure
  handling.

Completion criteria:

- Requirements and test cases cover repository configuration, package discovery,
  retrieval, cache behavior, unavailable repositories, and precedence.
- CI tests repository installation against a controlled test repository.

## Milestone 4: Package Signing and Validation

Define and implement a signed package format.

- Decide the canonical signed payload and signature-file format.
- Add build-time signing using a package maintainer's private key.
- Include the detached signature and signing-key identifier in the package.
- Validate the package signature and trusted signing key before installation.
- Reject unsigned, invalid, unknown-key, and revoked-key packages by default.
- Provide an explicitly documented administrator override for unsigned packages,
  if retained.

Completion criteria:

- Requirements and test cases cover successful signing and validation plus every
  rejection path.
- CI produces execution evidence for each signature test.
- The WPM self-package is signed in the release build process.

## Milestone 5: Version-Aware Upgrades

Implement `wpm upgrade <package...>` as a real package lifecycle operation.

- Discover installed package versions and available candidate versions.
- Compare versions using the SemVer rules in ADR-0004.
- Select only newer compatible packages.
- Download or obtain the selected package, validate its signature, install it,
  and retain enough state for removal and audit.
- Define behavior for no available upgrade, downgrade attempts, failed upgrades,
  and partial multi-package upgrades.

Completion criteria:

- A requirement specifies the upgrade contract and its version-selection rules.
- Test cases cover upgrade success, no-op when current, prerelease ordering,
  invalid or untrusted upgrades, rollback or failure recovery, and multi-package
  behavior.
- All upgrade test reports are included in the GitHub artifact.

## Milestone 6: Release Hardening

- Complete requirements-to-test traceability for all 1.0 commands.
- Run the full verification suite on every supported Windows architecture.
- Build, sign, and validate the release package in CI.
- Publish release notes, installation instructions, package-format guidance, and
  a 1.0 support policy.
- Tag and publish 1.0.0 only from a clean, fully verified commit.

## Release Gate

WPM 1.0.0 is ready only when every required test case passes in CI, its
execution evidence is attached to the build artifact, and the released package
is signed and validates successfully before installation.
