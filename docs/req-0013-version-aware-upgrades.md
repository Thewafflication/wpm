# REQ-0013: Version-Aware Upgrades

**Content type:** Project requirements

**Status:** Accepted

**Source:** ADR-0004 version and dependency semantics

## Scope

Applies to installed-state discovery, candidate selection, version comparison,
package upgrades, and upgrade recovery on supported Windows targets.

## Requirement

**REQ-0013.001**
WPM shall implement `wpm upgrade` as a version-aware package lifecycle
operation. Upgrade selection shall use installed package metadata, configured
HTTPS repositories, and Semantic Versioning 2.0 precedence.

### Package identity and installed-state discovery

**REQ-0013.002**
The retained ZIP archives beneath `%ProgramData%\WPM\packages` (or the
equivalent directory beneath `WPM_DATA_DIR`) shall be the authoritative local
records of installed packages. WPM shall read each retained archive's
`.wpm\package.txt` rather than infer package identity from its file name.

**REQ-0013.003**
An installed package identity consists of its case-insensitive package name and
its declared architecture. Supported architectures are `any`, `x86`, `x64`,
and `arm64`. Multiple specific architectures of the same package may coexist,
but an `any` installation shall not coexist with an architecture-specific
installation of the same package.

**REQ-0013.004**
Installation of an `any` package shall fail when a specific architecture of
that package is installed. Installation of a specific architecture shall fail
when the `any` architecture of that package is installed. The diagnostic shall
identify the conflict and require the conflicting installation to be removed
first.

**REQ-0013.005**
If legacy or externally modified state contains both `any` and specific
architectures for one package, WPM shall warn that the installed state is
inconsistent. Broad upgrade operations shall process the specific
architectures and ignore the `any` record. An explicit request to upgrade the
conflicting `any` identity shall fail until the conflict is resolved.

**REQ-0013.006**
When more than one retained version exists for the same package identity, the
highest valid SemVer precedence shall be its current installed version for
upgrade selection. Older retained versions shall remain independent,
version-specific records and shall not be modified by an upgrade.

**REQ-0013.007**
An invalid or unreadable installed record shall be reported and shall not be
silently interpreted as another package or version.

### Command forms and selectors

**REQ-0013.008**
WPM shall support:

```text
wpm upgrade <package...> [--arch <any|x86|x64|arm64>] [--version <semver>]
wpm upgrade --all [--arch <any|x86|x64|arm64>] [-y|--yes]
```

**REQ-0013.009**
Package operands identify installed packages by metadata name. A ZIP path or
archive file name is not an upgrade operand. Duplicate package operands shall
be processed once.

**REQ-0013.010**
Without `--arch`, WPM shall evaluate every installed architecture of each
named package independently. `--arch` shall restrict the operation to the
specified installed architecture. The operation shall fail for a named package
when the requested architecture is not installed.

**REQ-0013.011**
Without `--version`, WPM shall select the highest eligible newer version.
`--version` shall require the exact valid SemVer version specified and shall
not fall back to another version. The requested version must have greater
SemVer precedence than the current installed version; `upgrade` shall not
perform a downgrade or reinstall a version of equal precedence.

**REQ-0013.012**
`--all` shall discover and evaluate every valid installed package identity.
Package operands and `--all` are mutually exclusive. `--version` shall not be
accepted with `--all`. `--arch` may restrict `--all` to one installed
architecture.

**REQ-0013.013**
`--offline`, `--allow-unsigned`, and `--verbose` shall retain their existing
repository, signature-validation, and reporting meanings during upgrades.
Before an `--all` upgrade changes any package, WPM shall display every planned
identity with its installed and candidate versions and prompt once for
confirmation. A response other than `y` or `yes` shall cancel successfully
without acquisition or script execution. `-y` and `--yes` shall accept the
displayed plan without prompting; they shall not suppress the plan itself.

### Installation selectors

**REQ-0013.014**
Named installation shall additionally support:

```text
wpm install <package...> [--arch <any|x86|x64|arm64>] [--version <semver>]
```

**REQ-0013.015**
Without `--arch`, named installation shall use the existing native-system
architecture selection with `any` as a compatible fallback. `--arch` shall
select only the exact requested repository architecture, permitting, for
example, deliberate installation of an x86 package on an x64 system.

**REQ-0013.016**
Without `--version`, named installation shall select the highest eligible
version. `--version` shall select only the exact valid SemVer version and shall
fail rather than fall back when that version is unavailable or ineligible.
Installation may explicitly select a version older than other repository
versions because it is not an upgrade operation.

**REQ-0013.017**
A selector applies to every package operand in the same invocation. Different
selectors for different packages require separate invocations. `--arch` and
`--version` shall be rejected for ZIP-path installation because a local archive
already specifies its identity.

### Architecture-aware upgrade selection

**REQ-0013.018**
An upgrade candidate's architecture shall exactly equal the installed package
identity's architecture. WPM shall not use an `any` candidate as a fallback
for an installed `x86`, `x64`, or `arm64` package, and shall not use a specific
architecture candidate for an installed `any` package. Upgrade shall not
implicitly migrate a package between architectures.

**REQ-0013.019**
Thus, when both x86 and x64 identities are installed, an unqualified upgrade
shall resolve and upgrade each independently when an eligible candidate exists.
One identity may be upgraded while another is current or fails.

### Prerelease configuration

**REQ-0013.020**
Prerelease candidates shall be disabled by default. WPM shall provide a global
setting and an optional case-insensitive per-package override:

```text
wpm config set prerelease <true|false>
wpm config get prerelease
wpm config set prerelease <true|false> --package <name>
wpm config get prerelease --package <name>
wpm config unset prerelease --package <name>
```

**REQ-0013.021**
Configuration shall be stored beneath the WPM data root's `config` directory.
A per-package value shall take precedence over the global value. Removing an
override shall restore inheritance from the global value. A package-specific
`get` shall report the effective value and whether it comes from a package
override or the global setting.

**REQ-0013.022**
When prereleases are disabled for a package, prerelease repository candidates
shall be excluded from install and upgrade selection. Stable candidates remain
eligible even when the installed version is a prerelease, allowing an
installed prerelease to advance to a stable release. When prereleases are
enabled, stable and prerelease candidates shall participate according to normal
SemVer precedence. An exact prerelease requested with `--version` remains
ineligible when prereleases are disabled and shall fail with an explanatory
diagnostic.

### Semantic version selection

**REQ-0013.023**
Repository and installed versions participating in selection shall be valid
Semantic Versioning 2.0 versions. WPM shall compare their precedence according
to SemVer 2.0, including numeric and lexical prerelease identifier rules.
Build metadata shall not affect precedence and a build-metadata difference
alone shall not cause an upgrade.

**REQ-0013.024**
After architecture and prerelease filtering, WPM shall choose the candidate
with the highest version precedence. When equivalent versions occur in more
than one repository, the highest numeric repository priority shall win; equal
priorities shall be resolved by repository-add order as defined by REQ-0011.
Invalid repository versions shall be ignored with a warning and shall not
prevent selection from otherwise usable entries.

### Upgrade execution

**REQ-0013.025**
For an interactive `upgrade --all`, WPM shall accept `y` or `yes` followed by
Enter on standard input and proceed with the displayed plan. Any other response
or end-of-file shall cancel without changing packages. `-y` and `--yes` shall
continue to bypass the prompt for unattended use. Confirmation shall work with
both a Windows console input handle and redirected standard input.

**REQ-0013.026**
For each package identity selected for upgrade, WPM shall:

1. determine its current installed version,
2. resolve or confirm the eligible candidate,
3. obtain the candidate using the cache, refresh, offline, and failure rules
   in REQ-0011,
4. stage the candidate archive,
5. verify its package index and signature under REQ-0012,
6. confirm that staged name, version, and architecture exactly match the
   selected repository entry,
7. execute the candidate's `.wpm\install.cmd`, when present,
8. stream the candidate script output with interactive color preserved and
   automatically log its color-free text and exit code,
9. retain the successfully installed candidate archive and its repository URL,
10. write an upgrade audit record linking the prior and candidate identities,
   versions, archive names, verification result, and signing key, and
11. remove the staging directory after success or failure.

**REQ-0013.027**
All available validation shall complete before `install.cmd` is invoked. WPM
shall not invoke the prior version's `remove.cmd` as part of upgrade. Package
maintainers remain responsible for making `install.cmd` safely perform any
software-specific replacement or migration.

**REQ-0013.028**
The prior retained archive shall not be deleted after a successful upgrade.
It remains a version-specific removal and audit record consistent with WPM's
multiple-version package model.

**REQ-0013.029**
When upgrading the `wpm` package itself, the running installed executable shall
not attempt to overwrite itself. After validating the candidate package, WPM
shall copy the candidate `wpm.exe` into a collision-resistant handoff directory
beneath `cache\self-upgrade`, copy any
optional sidecar runtime supplied by a legacy package, launch the cached
executable as an independent completion process that inherits the caller's
standard streams, and exit. The completion process shall report when it begins
waiting for the invoking process, when it begins installation, and its final
`upgraded` or `failed` result. Official Release
packages shall contain a self-contained `wpm.exe` that can start from the cache
without a sidecar runtime. This preserves compatibility with earlier WPM
versions whose handoff logic copies only the executable. A Release package
shall still carry a compatibility sidecar when the immediately previous
supported release requires that file to validate or schedule its handoff. The completion
process shall wait for the invoking WPM process to terminate before repeating
normal package validation and installation. The completed upgrade shall retain
the candidate archive and create the normal upgrade audit record. The invoking
process shall report the result as `scheduled`, not `upgraded`, and identify a
persistent self-upgrade output log. That log shall contain package-script output
and the completion process's final `upgraded` or `failed` result.
When the invoking command is verbose, the completion process shall remain
verbose so its package-script process and wait diagnostics remain visible.
The completion process shall also accept the legacy eight-argument handoff
protocol used by earlier WPM releases; that protocol has no output-log path.

**REQ-0013.030**
Before an official release is published, every supported architecture shall
complete an isolated self-upgrade from the immediately previous published WPM
release to the candidate package. The test shall use the previous release's
executable to select, validate, cache, launch, and install the candidate; merely
invoking the candidate's completion command is insufficient.

**REQ-0013.031**
Package installation and upgrade scripts shall inherit WPM's standard input,
output, and error streams so their diagnostics are visible to an interactive
caller or captured by normal output redirection. Verbose output shall identify
the WPM process ID, package-script process ID, and the wait relationship between
them before WPM waits for script completion. While a package script remains
blocked, verbose output shall periodically report its elapsed runtime, visible
direct child processes, and a command for inspecting the script process.
WPM shall expose verbose mode to package scripts through `WPM_VERBOSE=1`. WPM's
setup script shall use it to report registry targets, intended changes, Path
update decisions, and completed registry-operation exit codes.
The setup script shall not rewrite the persistent Path when a WPM entry is
already present.

### Current packages and failed upgrades

**REQ-0013.032**
When no eligible candidate has greater precedence than the current installed
version, WPM shall report that identity as current, perform no package download
or script execution, and treat the result as a successful no-op. Verbose output
shall identify when otherwise matching prereleases were excluded by
configuration.

**REQ-0013.033**
If acquisition or validation fails, WPM shall not invoke `install.cmd` or
record the candidate as installed. If `install.cmd` fails, WPM shall not retain
the candidate as a successfully installed package. In either case, prior
archives and audit records shall remain intact and the staging directory shall
be removed. A script failure shall print its log path, the selected repository
URL, and a prompt containing the repository's GitHub issue URL when applicable.

**REQ-0013.034**
WPM shall write a failed-upgrade audit record when an upgrade attempt has
selected a candidate. It shall include the package identity, old and candidate
versions, failure phase, and script exit code when applicable.

**REQ-0013.035**
WPM cannot guarantee rollback of arbitrary changes made by package scripts. A
script failure shall warn that package-maintainer recovery may be required. If
archive retention or audit recording fails after `install.cmd` succeeds, WPM
shall report a recovery-required failure and shall not claim that deployed
software was rolled back.

### Multiple-package results

**REQ-0013.036**
Packages and architectures shall be processed independently. Failure of one
identity shall not prevent later identities in the invocation from being
attempted. WPM shall produce a final result for each identity as `upgraded`,
`current`, or `failed`.

**REQ-0013.037**
The process exit status shall be zero only when every selected identity is
either upgraded or current. Any failed identity shall produce a nonzero exit
status. Multi-package upgrade is best-effort and shall not claim transactionality
or cross-package rollback.

### Installer file users

**REQ-0013.038**
WPM shall provide `file-processes <file...>` to list the PID and application
name of processes using exact destination file paths. Missing files shall be
ignored; directories, wildcard paths,
missing operands, and unknown options shall fail before termination begins.
This optional utility shall use Restart Manager when available and provide a
Windows 2000/XP fallback using loaded-module and disk-file-handle enumeration,
without introducing static imports of newer operating-system APIs. `--legacy`
shall select the fallback explicitly for diagnostics and verification.

**REQ-0013.039**
Termination shall require explicit `--kill`; `--dry-run` shall prevent it.
The utility shall check process creation time on the handle used to terminate,
wait for exit, and refuse services, Explorer, critical processes, itself, and
its ancestors. Failures to query or stop processes, and remaining live file
users after termination, shall return nonzero. Listing shall return zero on
successful discovery, including an empty result.
Legacy enumeration shall warn about inaccessible or cross-architecture processes
and shall not claim that its accessible-process snapshot proves replaceability.
Service-PID inspection shall be required before legacy termination. Legacy
critical-process protection shall include known system-image names and the
critical-process flag where the operating system exposes it.

**REQ-0013.040**
The utility shall accept multiple paths, report each affected process once,
avoid matching by executable basename, and operate without initializing WPM
package or data directories. It shall not automatically restart applications.

### Confirmed installer copies

**REQ-0013.041**
WPM shall provide `copy <source> <destination>` and accept equivalent
`--recursive` and `-r` options for directory sources. Directory contents,
including empty subdirectories, shall be merged into the exact destination;
destination-only files shall be retained. Single files shall support an exact
destination filename or an existing directory destination. Copy failures shall
return nonzero and shall not claim atomicity or rollback.

**REQ-0013.042**
The copy utility shall validate and display the plan and affected file users
before prompting by default. An affirmative answer shall authorize stopping
ordinary applications and copying/overwriting; negative input, empty input and
EOF shall cancel without termination or copying. `--yes`/`-y` shall explicitly
authorize unattended execution. `--dry-run` shall prevent termination and all
filesystem mutation, including with `--yes` or `--kill`. Copy shall reuse the
file-user backends and identity/protected-process checks, require write/script
capabilities, and avoid initializing WPM package/data directories.

**REQ-0013.043**
Copy planning shall reject overlapping roots, same-file source/destination
identities, reparse points in source/destination paths, unsupported or ambiguous
names, file/directory conflicts and traversal/enumeration failures before
termination. It shall expand existing short-name aliases and bound traversal
and path lengths. It shall recheck the planned names/types after confirmation
and fail if they changed. Actual copy errors shall remain authoritative;
snapshot checks shall not imply protection against concurrent filesystem changes.

## Rationale

Version-aware upgrades must distinguish installed package identities from the
architecture of the WPM executable, because x86 software is commonly deployed
on x64 Windows. Exact architecture matching prevents upgrades from silently
changing a package's deployment architecture. Configurable prerelease
selection supports both stable systems and development channels while keeping
stable behavior as the default.

Retained archives provide the only general package state that WPM can own;
package scripts may deploy arbitrary files, services, registry settings, MSI
packages, or drivers. Prevalidation, durable audit records, and explicit
recovery reporting are therefore enforceable, while universal rollback is not.

## Verification

**Method:** Automated test and inspection

**References:** TC-0013, `tests/tc-0013-*.ps1`, and
`tests/file-processes-integration.py` (installer file-user companion test) and
`tests/copy-integration.py` (confirmed recursive copy companion test)

Verified by:

- TC-0013 - Version-aware package installation and upgrades

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0013-version-aware-upgrades.ps1` currently implement and verify this requirement.
