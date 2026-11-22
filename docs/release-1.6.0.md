# WPM 1.6.0 - dependency repository hints

## Scope and authorization

The maintainer requested the feature, review and commit of other appropriate
pending work, and release. This compatible 1.x increment adds an optional
`;repo=locator` suffix to `depends=name>=version` declarations. Existing
metadata and command defaults remain supported. Packages using the new suffix
require WPM 1.6.0 or newer; older clients reject that dependency syntax.

When installed/planned packages and configured catalogs cannot satisfy a
requirement, WPM displays its repository hint and asks to add the repository,
refresh indexes and search again. `-y` / `--yes` accepts automatically.
Decline or EOF fails before graph scripts execute. Each unresolved declaration
gets at most one retry. The added repository remains configured if resolution
still fails. Previously selected graph identities and source locators survive
catalog reloads. Offline and dry-run modes report the hint without mutation.

HTTPS and filesystem locators use existing repository validation. Hints do not
authorize insecure HTTP or trust a signing key. Package signatures, prerelease
policy, architecture selection, graph limits and separate WPM self-upgrade
rules still apply. The 1023-byte aggregate dependency metadata limit includes
repository hints. No rollback or reverse-dependency protection is added.

## Impact and verification

REQ-0013.044-.046 and TC-0013's dependency integration companion cover this
extension. The ADR-0004 implementation allocation and usage guide describe the
new syntax and fallback. Regression cases cover automatic and interactive
acceptance, decline/EOF, unchanged offline/dry-run configuration, installed
satisfaction, retained earlier graph selections, an empty retry catalog and
malformed hints. The x64 Release build and focused dependency, operation and
dry-run integration suites passed before the commit checks.

Required publication gates are the normal lint, x64 Debug build and fresh full
CTest hooks, then tag-triggered native x86/x64/ARM64 verification, Release
builds, signed packages, previous-stable upgrades, documentation checks and
publication. A failure blocks publication; no assertions or timeouts are waived.
The annotated 1.6.0 tag identifies the exact release source. GitHub Release CI
retains native test reports and artifact provenance; published assets and their
checksums identify the final delivered bytes.

The accompanying 1.5.1 roadmap reconciliation is historical evidence only.
Its 77 obligations retain six Verified static rows and 71 Planned runtime rows.
The retained ARM64 CLR-runner failures remain open findings, not claimed fixes.
Scratch outputs, local Python packages and stale task bookkeeping are excluded.
