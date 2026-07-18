# WPM 2.0 Release Roadmap

## Goal

Make WPM easier to understand, safer to operate, and quicker to recover when a
package operation fails, while preserving the signed-package, repository, and
upgrade foundations established in 1.0.

## Principles

- Keep commands scriptable: prompts must have a documented non-interactive
  option and output must remain useful in CI.
- Explain intent before making a change: show the package, architecture,
  version, source, and affected files when practical.
- Make errors actionable: identify the failed phase, relevant path or
  repository, and the next useful command.
- Work offline by design: repository operations must be usable from local and
  removable media without requiring a network connection.
- Preserve the 1.x package, signature, repository-index, and deployment
  contracts unless a migration is explicitly documented.

## Milestone 1: Command Output and Help

- Rewrite command help around common tasks, with short examples for install,
  update, upgrade, trust, repository management, and recovery.
- Use consistent success, warning, error, and result summaries across commands.
- Add color when output is connected to an interactive terminal: distinct,
  accessible styling for progress, success, warning, error, prompts, and
  package-script output; provide `--color auto|always|never`.
- Add concise progress indicators for repository access, copying, downloads,
  archive work, validation, and package-script phases without making normal
  output noisy. Non-interactive output must remain line-oriented and stable.
- Expand `--verbose` so every mutating operation reports its selected
  repository or source, resolved package identity, cache/staging locations,
  validation steps, script invocation, retained artifacts, and final result.
- Keep normal output concise; verbose output must add operational detail rather
  than merely repeat normal messages.
- Add structured, timestamped operational logging with configurable verbosity,
  a documented log location, and command output that points to the relevant log
  after a failure.
- Improve invalid-argument messages to show the relevant usage form.

Completion criteria:

- Every public command has a task-oriented help example.
- Automated tests verify color selection, non-interactive output, progress,
  logging, common help, error, and result-summary paths.
- Verbose-mode tests verify that each lifecycle phase is identifiable without
  exposing private keys, credentials, or other secrets.

## Milestone 2: Safer Package Changes

- Show a complete install, remove, and upgrade plan before confirmation.
- Clearly distinguish changes that are downloaded, verified, staged, installed,
  retained, or scheduled for completion after WPM exits.
- Add `--dry-run` to supported mutating commands.
- Make the prompt default safe and make the bypass option consistent across
  commands.
- Report package-script output in a readable, clearly bounded section.

Completion criteria:

- Users can determine the exact planned change before approving it.
- A dry run performs no mutable package, cache, trust, or deployment change.

## Milestone 3: Local, Network-Share, and Legacy-Network Repositories

- Support local filesystem repository roots in addition to HTTPS, including
  absolute paths, drive roots, USB/removable drives, and CD-ROM media.
- Support SMB/UNC repository roots for managed local-network distribution.
- Support plain HTTP repositories for legacy environments where HTTPS is not
  practical, including Windows XP-era systems. Plain HTTP must require an
  explicit opt-in and display a clear transport-security warning.
- Support read-only repositories so a signed repository can be distributed on
  optical media or a locked USB drive.
- Keep repository URLs and local paths explicit in output, diagnostics, and
  audit records so users can tell whether a package came from the network or
  local media.
- Retain the existing signature and trust validation rules for every transport;
  local media, SMB, and HTTP do not imply trusted packages or signing keys.
- Define behavior for removable media that is absent, changes drive letter, or
  becomes unavailable while an operation is in progress.

Completion criteria:

- A user can add, list, update, and install from fixed disk, USB, CD-ROM, SMB,
  HTTPS, and explicitly opted-in HTTP repositories.
- Automated tests cover writable and read-only local repositories, unavailable
  media, SMB behavior, HTTP opt-in/warnings, and signature validation from each
  supported transport.

## Milestone 4: Repository Authoring

- Add `wpm repo init <directory>` to create a repository layout and guidance.
- Add `wpm repo add-package <repository> <archive>` to validate and copy a
  package into a writable repository.
- Add `wpm repo index <repository>` to build or refresh `index.json`, and
  support signing the resulting index with a configured maintainer key.
- Add `wpm repo verify <repository>` to validate repository layout, index
  entries, package availability, and signatures before distribution.
- Document a repeatable workflow for authoring a repository on a local disk,
  then copying it to USB or mastering it to CD-ROM.

Completion criteria:

- WPM can create, populate, index, sign, verify, and consume a repository
  without external repository-authoring tools.
- A generated repository works unchanged after being copied to removable media.

## Milestone 5: Discoverability and Diagnostics

- Expand `wpm --diagnose` into a concise health report covering runtime mode,
  data locations, configured repositories, cache state, trusted keys, and
  pending self-upgrade work.
- Add package-list views with consistent table-like output:
  - `wpm list` lists installed package identities, architecture, current
    version, retained-version count, and upgrade state;
  - `wpm list --available` lists eligible repository packages after repository
    selection and prerelease policy are applied; and
  - filtering by package name, architecture, repository, and version makes
    large package lists usable.
- Add `wpm show <package>` to explain one package identity without changing it.
  It shall display installed architectures and retained versions, eligible
  repository candidates, selected source/repository, signature trust state,
  prerelease eligibility, available upgrade, and relevant audit or recovery
  information. `--arch` and `--version` shall narrow the view; ambiguous
  requests shall show the available choices instead of silently selecting one.
- Provide a stable machine-readable output mode for lists and `show`, alongside
  the interactive table format.
- Provide remediation guidance for common conditions: no repository, missing
  trusted key, unavailable repository, invalid installed record, conflicting
  architectures, and a pending self-upgrade.

Completion criteria:

- A user can collect the information needed for a support report with one
  command.
- Users can discover installed and available packages, then explain an exact
  package selection with `wpm show`, without attempting installation or upgrade.
- Each diagnosed unhealthy condition includes a documented next action.

## Milestone 6: Recovery and Lifecycle Polish

- Make interrupted installs, failed scripts, and self-upgrade failures easy to
  inspect and retry from their audit records.
- Provide a supported cleanup path for stale cache, staging, and legacy package
  records without deleting active installations.
- Improve uninstall messaging so it clearly distinguishes WPM binaries,
  mutable data, retained archives, and user configuration.
- Document backup and restore expectations for `%ProgramData%\WPM` and
  user-scoped data roots.

Completion criteria:

- Failed operations identify retained artifacts and a safe retry or cleanup
  command.
- Recovery behavior is covered by automated tests on x86, x64, and ARM64.

## Milestone 7: Quality Testing and Resilience

- Establish the [quality testing program](quality-testing.md) for endurance,
  fuzzing, malformed input, fault injection, removable-media, transport, and
  recovery testing.
- Keep these tests separate from the fast deterministic CI suite; run them
  manually, nightly, before release candidates, or in dedicated disposable
  environments as appropriate.
- Capture the command, environment, logs, seed/corpus input, retained
  artifacts, and minimized reproduction for every discovered defect.
- Promote reproducible quality failures into normal automated regression tests
  when they can be made deterministic and fast.

Completion criteria:

- A documented quality-test harness and regression corpus exist.
- Every 2.0 release candidate completes the quality-test release-candidate
  gate with recorded results and triaged findings.

## Milestone 8: Code Quality and Portability

- Make the WPM source portable C99: remove reliance on C11-only language and
  library features from the portable core, and isolate Windows-specific APIs
  behind narrow platform helpers.
- Add a C99 compilation check to CI in addition to the Windows release builds.
- Document public module interfaces, data formats, ownership rules, security
  boundaries, and non-obvious algorithms using Doxygen-compatible comments.
- Use Doxygen-style `/** ... */` comments for public functions, structures,
  constants, and module headers; use concise ordinary comments for local
  implementation details.
- Generate and publish API/reference documentation as a CI artifact when the
  documentation toolchain is available.

Completion criteria:

- The portable core compiles in a C99 configuration with warnings treated as
  errors for newly introduced code.
- Public interfaces and security-sensitive flows have Doxygen-compatible
  documentation, and generated reference documentation completes without
  warnings.

## Milestone 9: Release Experience

- Publish a concise migration guide from 1.x to 2.0.
- Add release notes organized by user-facing behavior, upgrade notes, and known
  limitations.
- Validate the 2.0 bootstrap installation and self-upgrade journeys from the
  previous stable release.
- Refresh the support policy and documentation screenshots/examples for 2.0.

## Non-goals for 2.0

- Dependency solving and package relationship resolution.
- A graphical user interface.
- Automatic trust of repository-distributed signing keys.

Remote transports beyond HTTPS, SMB, and explicitly opted-in plain HTTP, such
as SCP or other protocols, will be evaluated after the common repository
contract is stable.

## Compatibility Investigations

The [compatibility tracking document](compatibility-tracking.md) records the
separate investigation into an x86 Pentium III / Windows 2000-capable build.
It is intentionally not a WPM 2.0 delivery or support commitment.

Those may be evaluated after the 2.0 user experience is stable.

## Release Gate

WPM 2.0.0 is ready when all documented user journeys pass on x86, x64, and
ARM64; every mutating command has safe confirmation and non-interactive
behavior; recovery paths are verified; and signed release packages validate
before publication.
