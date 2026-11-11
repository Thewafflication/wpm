# ADR-0004: Dependency Resolution and Version Constraints

**Status:** Accepted

**Date:** 2026-06-15

## Context

WPM supports both tagged releases and Git-derived development builds.

## Decision Drivers

- Release and development versions need one parseable representation.
- Ordering must be deterministic and use established semantics.
- Published versions must distinguish clean releases from dirty builds.
- Dependency constraints need an extensible syntax.

## Considered Options

1. Semantic Versioning 2.0 with prerelease and build metadata.
2. Lexicographic arbitrary versions.
3. Date-based versions.
4. A project-specific numeric scheme.

## Decision

WPM SHALL use Semantic Versioning 2.0.

Stable releases:

1.0.0
1.2.3
2.0.0

Development builds:

1.0.1-dev+17.9c52d1f
1.0.1-dev+18.ab12cd3
1.0.1-dev+18.ab12cd3.dirty

Where:

- `dev` identifies a development prerelease.
- `+<n>.<hash>` is build metadata containing the commit count since the last
  tag and the Git commit hash.
- .dirty indicates uncommitted changes.

## Version Ordering

Example ordering:

1.0.0-dev+1.abc123
and
1.0.0-dev+2.def456
have equal SemVer precedence because build metadata does not affect ordering.
Both are less than

1.0.0
<
1.0.1-dev+1.789abc
<
1.0.1

WPM SHALL compare versions according to SemVer 2.0.

## Rationale

Semantic Versioning provides a documented precedence model and expresses stable
and prerelease identities while retaining non-precedence build provenance.
Custom, lexical, or date-based schemes would require additional ecosystem rules.

## Dirty Builds

Versions containing 'dirty' MUST NOT be published to repositories.

Dirty builds SHOULD only be used for local development.

## Dependency Syntax

Examples:

depends=libfoo>=1.0.0
depends=libbar>=2.1.0

Future support:

depends=libfoo>=1.0.0,<2.0.0

## Dependency Resolution

The 1.4.0 implementation accepts repeated `depends=name>=version` lines in
`.wpm/package.txt`. Each version must be a complete SemVer. The aggregate
newline-separated declarations are bounded to 1023 bytes. Comma ranges,
upper bounds and alternative providers remain future work and are rejected.
Compatible retained installed metadata satisfies requirements without an
implicit update. Otherwise resolution chooses the highest compatible eligible
catalog version, preferring the parent's specific architecture over `any` and
using existing repository priority/order ties. All minimum constraints are
monotone; a general SAT solver is unnecessary. Cycles, conflicting selected
versions and graphs over 64 selected packages fail explicitly. Previously
planned roots are fixed; later requirements cannot silently replace them.
Dependencies execute in authored depth-first topological order and share the
existing immutable confirmation plan. ZIP installs and repository upgrades
use the same resolver. Required WPM upgrades must be performed explicitly
before deploying their dependents. See REQ-0013.044 through REQ-0013.046.

WPM SHALL select the highest compatible version.

Resolution SHALL be deterministic.

## Upgrade Rules

Stable releases SHALL be preferred over prerelease builds of the same version.

Example:

Installed:
1.0.0-dev+17.9c52d1f

Available:
1.0.0

Result:
Upgrade permitted.

## Consequences

### Positive

- Stable and development identities share a standard syntax.
- Selection and dependency resolution can be deterministic.

### Negative

- Build metadata cannot order otherwise equal SemVer versions.
- Package authors must follow SemVer prerelease rules.

### Follow-up

- Maintain boundary and precedence tests for parsing and selection.
- Record expanded dependency-range semantics before implementation.

## References

- REQ-0013 and TC-0013
- Semantic Versioning 2.0.0
