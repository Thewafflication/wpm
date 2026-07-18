# REQ-0013: Version-Aware Upgrades

## Description

WPM shall implement `wpm upgrade` as a version-aware package lifecycle
operation. Upgrade selection shall use installed package metadata, configured
HTTPS repositories, and Semantic Versioning 2.0 precedence.

### Package identity and installed-state discovery

The retained ZIP archives beneath `%ProgramData%\WPM\packages` (or the
equivalent directory beneath `WPM_DATA_DIR`) shall be the authoritative local
records of installed packages. WPM shall read each retained archive's
`.wpm\package.txt` rather than infer package identity from its file name.

An installed package identity consists of its case-insensitive package name and
its declared architecture. Supported architectures are `any`, `x86`, `x64`,
and `arm64`. Multiple specific architectures of the same package may coexist,
but an `any` installation shall not coexist with an architecture-specific
installation of the same package.

Installation of an `any` package shall fail when a specific architecture of
that package is installed. Installation of a specific architecture shall fail
when the `any` architecture of that package is installed. The diagnostic shall
identify the conflict and require the conflicting installation to be removed
first.

If legacy or externally modified state contains both `any` and specific
architectures for one package, WPM shall warn that the installed state is
inconsistent. Broad upgrade operations shall process the specific
architectures and ignore the `any` record. An explicit request to upgrade the
conflicting `any` identity shall fail until the conflict is resolved.

When more than one retained version exists for the same package identity, the
highest valid SemVer precedence shall be its current installed version for
upgrade selection. Older retained versions shall remain independent,
version-specific records and shall not be modified by an upgrade.

An invalid or unreadable installed record shall be reported and shall not be
silently interpreted as another package or version.

### Command forms and selectors

WPM shall support:

```text
wpm upgrade <package...> [--arch <any|x86|x64|arm64>] [--version <semver>]
wpm upgrade --all [--arch <any|x86|x64|arm64>] [-y|--yes]
```

Package operands identify installed packages by metadata name. A ZIP path or
archive file name is not an upgrade operand. Duplicate package operands shall
be processed once.

Without `--arch`, WPM shall evaluate every installed architecture of each
named package independently. `--arch` shall restrict the operation to the
specified installed architecture. The operation shall fail for a named package
when the requested architecture is not installed.

Without `--version`, WPM shall select the highest eligible newer version.
`--version` shall require the exact valid SemVer version specified and shall
not fall back to another version. The requested version must have greater
SemVer precedence than the current installed version; `upgrade` shall not
perform a downgrade or reinstall a version of equal precedence.

`--all` shall discover and evaluate every valid installed package identity.
Package operands and `--all` are mutually exclusive. `--version` shall not be
accepted with `--all`. `--arch` may restrict `--all` to one installed
architecture.

`--offline`, `--allow-unsigned`, and `--verbose` shall retain their existing
repository, signature-validation, and reporting meanings during upgrades.
Before an `--all` upgrade changes any package, WPM shall display every planned
identity with its installed and candidate versions and prompt once for
confirmation. A response other than `y` or `yes` shall cancel successfully
without acquisition or script execution. `-y` and `--yes` shall accept the
displayed plan without prompting; they shall not suppress the plan itself.

### Installation selectors

Named installation shall additionally support:

```text
wpm install <package...> [--arch <any|x86|x64|arm64>] [--version <semver>]
```

Without `--arch`, named installation shall use the existing native-system
architecture selection with `any` as a compatible fallback. `--arch` shall
select only the exact requested repository architecture, permitting, for
example, deliberate installation of an x86 package on an x64 system.

Without `--version`, named installation shall select the highest eligible
version. `--version` shall select only the exact valid SemVer version and shall
fail rather than fall back when that version is unavailable or ineligible.
Installation may explicitly select a version older than other repository
versions because it is not an upgrade operation.

A selector applies to every package operand in the same invocation. Different
selectors for different packages require separate invocations. `--arch` and
`--version` shall be rejected for ZIP-path installation because a local archive
already specifies its identity.

### Architecture-aware upgrade selection

An upgrade candidate's architecture shall exactly equal the installed package
identity's architecture. WPM shall not use an `any` candidate as a fallback
for an installed `x86`, `x64`, or `arm64` package, and shall not use a specific
architecture candidate for an installed `any` package. Upgrade shall not
implicitly migrate a package between architectures.

Thus, when both x86 and x64 identities are installed, an unqualified upgrade
shall resolve and upgrade each independently when an eligible candidate exists.
One identity may be upgraded while another is current or fails.

### Prerelease configuration

Prerelease candidates shall be disabled by default. WPM shall provide a global
setting and an optional case-insensitive per-package override:

```text
wpm config set prerelease <true|false>
wpm config get prerelease
wpm config set prerelease <true|false> --package <name>
wpm config get prerelease --package <name>
wpm config unset prerelease --package <name>
```

Configuration shall be stored beneath the WPM data root's `config` directory.
A per-package value shall take precedence over the global value. Removing an
override shall restore inheritance from the global value. A package-specific
`get` shall report the effective value and whether it comes from a package
override or the global setting.

When prereleases are disabled for a package, prerelease repository candidates
shall be excluded from install and upgrade selection. Stable candidates remain
eligible even when the installed version is a prerelease, allowing an
installed prerelease to advance to a stable release. When prereleases are
enabled, stable and prerelease candidates shall participate according to normal
SemVer precedence. An exact prerelease requested with `--version` remains
ineligible when prereleases are disabled and shall fail with an explanatory
diagnostic.

### Semantic version selection

Repository and installed versions participating in selection shall be valid
Semantic Versioning 2.0 versions. WPM shall compare their precedence according
to SemVer 2.0, including numeric and lexical prerelease identifier rules.
Build metadata shall not affect precedence and a build-metadata difference
alone shall not cause an upgrade.

After architecture and prerelease filtering, WPM shall choose the candidate
with the highest version precedence. When equivalent versions occur in more
than one repository, the highest numeric repository priority shall win; equal
priorities shall be resolved by repository-add order as defined by REQ-0011.
Invalid repository versions shall be ignored with a warning and shall not
prevent selection from otherwise usable entries.

### Upgrade execution

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
8. retain the successfully installed candidate archive,
9. write an upgrade audit record linking the prior and candidate identities,
   versions, archive names, verification result, and signing key, and
10. remove the staging directory after success or failure.

All available validation shall complete before `install.cmd` is invoked. WPM
shall not invoke the prior version's `remove.cmd` as part of upgrade. Package
maintainers remain responsible for making `install.cmd` safely perform any
software-specific replacement or migration.

The prior retained archive shall not be deleted after a successful upgrade.
It remains a version-specific removal and audit record consistent with WPM's
multiple-version package model.

When upgrading the `wpm` package itself, the running installed executable shall
not attempt to overwrite itself. After validating the candidate package, WPM
shall copy the candidate `wpm.exe` beneath `cache\self-upgrade`, launch that
cached executable as a detached completion process, and exit. The completion
process shall wait for the invoking WPM process to terminate before repeating
normal package validation and installation. The completed upgrade shall retain
the candidate archive and create the normal upgrade audit record. The invoking
process shall report the result as `scheduled`, not `upgraded`, and identify a
persistent self-upgrade output log. That log shall contain package-script output
and the completion process's final `upgraded` or `failed` result.

Package installation and upgrade scripts shall inherit WPM's standard input,
output, and error streams so their diagnostics are visible to an interactive
caller or captured by normal output redirection.

### Current packages and failed upgrades

When no eligible candidate has greater precedence than the current installed
version, WPM shall report that identity as current, perform no package download
or script execution, and treat the result as a successful no-op. Verbose output
shall identify when otherwise matching prereleases were excluded by
configuration.

If acquisition or validation fails, WPM shall not invoke `install.cmd` or
record the candidate as installed. If `install.cmd` fails, WPM shall not retain
the candidate as a successfully installed package. In either case, prior
archives and audit records shall remain intact and the staging directory shall
be removed.

WPM shall write a failed-upgrade audit record when an upgrade attempt has
selected a candidate. It shall include the package identity, old and candidate
versions, failure phase, and script exit code when applicable.

WPM cannot guarantee rollback of arbitrary changes made by package scripts. A
script failure shall warn that package-maintainer recovery may be required. If
archive retention or audit recording fails after `install.cmd` succeeds, WPM
shall report a recovery-required failure and shall not claim that deployed
software was rolled back.

### Multiple-package results

Packages and architectures shall be processed independently. Failure of one
identity shall not prevent later identities in the invocation from being
attempted. WPM shall produce a final result for each identity as `upgraded`,
`current`, or `failed`.

The process exit status shall be zero only when every selected identity is
either upgraded or current. Any failed identity shall produce a nonzero exit
status. Multi-package upgrade is best-effort and shall not claim transactionality
or cross-package rollback.

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

Verified by:

- TC-0013 - Version-aware package installation and upgrades
