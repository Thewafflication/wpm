# WPM 1.4.0 - required package dependency resolution

## Scope and authorization

The user authorized implementation, commit, push and release. Packages may
repeat `depends=name>=version` in `.wpm/package.txt`, using complete SemVer.
Repository installs, ZIP installs and repository upgrades resolve required
packages before deploying their dependents. Compatible retained installed
metadata or earlier planned operations satisfy minimum requirements; otherwise
the highest eligible compatible repository version is selected, preferring the
parent's architecture over `any` and honoring prerelease policy and repository
priority/order. Insufficient retained versions use the matching-architecture
upgrade lifecycle.

The resolver validates complete selected graphs and holds archive read handles
before adding dependency-first items to the existing immutable approval plan.
Shared dependencies run once. Cycles, missing requirements, incompatible
selected versions, invalid syntax and more than 64 selected packages fail.
Metadata declarations are bounded to 1023 aggregate bytes. Failure stops parent
deployment; completed package scripts are not rolled back. Required WPM upgrades
must be performed explicitly first. Upper bounds, alternatives, bundles,
conflicts, recommendations and reverse-dependency removal protection remain
outside this release. Packages without dependencies keep existing behavior.

REQ-0013.044 through REQ-0013.046 and the TC-0013 dependency integration
companion allocate the change. ADR-0004 and ADR-0006 record its semantics and
remaining design targets. See the usage guide for syntax and operational limits.

## Verification and publication criteria

The normal lint, build and full fresh CTest commit checks must pass. The
new integration exercises signed real packages, transitive diamond graphs,
multiple roots, installed satisfaction, dependency upgrades, dry-run and
cancellation, offline ZIP installation, cycles, missing versions/architectures,
script failure, malformed declarations and the 64-node limit.

The release tag identifies the final source revision. Tag-triggered Release CI
must pass x86/x64/native ARM64 verification, signed packaging, previous-release
upgrades and documentation verification/provenance before publication. Expected
assets are three signed architecture packages, index, release public key,
bootstrap scripts, engineering PDF and SHA-256 checksum. Published bytes,
package signatures and PDF identity must be checked before completion.

The engineering PDF is rendered and reviewed before tagging. Exact page-body
matches may reuse the previously approved 1.3.6 rendering; changed bodies must
be visually inspected. Review evidence is retained under `out/release-1.4.0-review`.
Native legacy Windows execution remains unverified; support policy is unchanged.

The local candidate PDF passed structure, identity and checksum validation with
360 pages and 1,080 links. All page bodies were reviewed: 327 match the approved
1.3.6 rendering exactly and all 33 changed bodies were inspected, including
usage and requirements at higher resolution. Independent text-boundary checks
found zero exceptions. Final CI and published documentation still require
verification against the tagged source and asset bytes.

Current-host x64 Debug passed 61/61 fresh CTest tests in 305.28 seconds.
The dependency integration passed independently on x64 and x86. The x86 PE
check passed with OS/subsystem 4.0 and 128 kernel32 imports. Source style,
C99, traceability and WSP adoption checks passed. Installed commit hooks and
tag-triggered CI remain mandatory publication gates.

Parallel CTest runs isolate the two legacy process-enumeration integrations
with `RUN_SERIAL`: they scan all live processes and must not compete with
process creation by unrelated tests. An initial commit-hook run exposed a
60-second command timeout in the existing legacy copy integration while the
other 60 tests passed. No timeout or assertion was relaxed; fresh mandatory
hooks must pass with this scheduling fix before publication.
