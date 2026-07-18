# ADR-0004: Dependency Resolution and Version Constraints

Status: Accepted

## Context

WPM supports both tagged releases and Git-derived development builds.

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
