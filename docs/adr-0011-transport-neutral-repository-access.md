# ADR-0011: Transport-Neutral Repository Access

**Status:** Accepted

**Date:** 2026-08-04

**Relationship:** Supersedes ADR-0005 only for the required provider set,
locator syntax, and repository-read boundary in WPM 2.0. ADR-0005's common
version-1 logical repository, cache, priority, metadata, and separation of
transport from trust remain accepted.

## Context

ADR-0005 selected interchangeable providers but described URI-like `file`,
SMB, HTTPS, and SCP examples without defining parsing, capabilities, media
identity, or insecure-transport policy. Accepted REQ-0011 then intentionally
limited 1.x to HTTPS. WPM 2.0 requires the same logical repository on local and
removable filesystems, read-only optical media, UNC shares, HTTPS, and explicitly
enabled HTTP.

## Decision Drivers

- Transport cannot authorize content or a signer.
- Windows paths must not be misclassified as URLs.
- Read-only and removable sources must never be modified.
- Cache replacement must survive absence, partial reads, and source changes.
- HTTP is needed for constrained environments but must remain explicit and
  conspicuous.
- The version-1 repository and package formats must remain consumable.

## Considered Options

1. A typed locator and narrow read-only repository-source interface.
2. Scheme substring tests spread through command code.
3. Copy every source to a writable local mirror before validation.
4. A separate repository format for each transport.

## Decision

WPM SHALL parse a configured locator once into a typed value and expose a
transport-neutral, read-only source interface above it. The interface provides
bounded open/read of a logical repository object, availability and immutable
source identity for the operation, and transport-specific diagnostic context.
Cache, version, priority, candidate selection, index parsing, signature, and
package trust remain above this interface.

The required 2.0 locator kinds are:

- absolute drive or rooted filesystem paths, including drive roots;
- UNC paths using the caller's Windows-authenticated SMB access;
- `https://` URLs; and
- `http://` URLs with repository-scoped explicit opt-in.

Windows drive and UNC grammar is resolved before URI parsing. URL kinds require
a parsed, allow-listed scheme and authority; substring heuristics are forbidden.
Relative roots, device namespaces, ambiguous slash forms, URL user information,
and unsupported schemes are rejected. SCP is not in the required 2.0 provider
set and ADR-0005's earlier SCP requirement no longer governs 2.0; adding it later
requires identified requirements and security review.

The configured locator is preserved for display and administration. A separate
canonical comparison identity and a redacted effective locator are used for
deduplication, access, logging, audit, and recovery. Credentials and tokens are
never persisted as part of a locator.

## Source and Cache Boundary

Index and archive paths are resolved under the selected logical root. Relative
version-1 paths remain portable. Existing absolute HTTPS package URLs remain
valid only for HTTPS repositories under REQ-0011; authored portable repositories
use relative paths. Traversal, absolute filesystem paths from index content,
device paths, and scheme changes are rejected before access.

Each operation snapshots source identity and relevant object identity before
approval and rechecks them before committing cache or package state. Local media
identity includes the resolved volume and root; WPM does not search other drive
letters or silently substitute a volume. A missing, replaced, disconnected, or
changed source invalidates the plan.

Source data is read into operation-specific temporary files only when required.
An existing usable cache is replaced atomically only after the complete new
object passes applicable bounds, format, signature, identity, and trust checks.
Neither the source interface nor repository verification writes to the source.

## HTTP Opt-In and Redirects

Adding an HTTP repository requires `--allow-insecure-http`. The resulting
configuration stores a Boolean permission scoped to that canonical repository;
there is no global enable switch and HTTPS repositories do not inherit it. Each
HTTP access emits the required human warning and records the transport in safe
diagnostic, audit, and recovery fields.

A redirect is reparsed as a new locator. HTTP permission does not authorize a
redirect to another origin or unsupported scheme, and HTTPS never silently
downgrades to HTTP. Transport permission never relaxes repository-index,
package-signature, manifest, or local signer authorization.

## Compatibility and Migration

REQ-0016 is the explicit 2.0 superseding requirement for REQ-0011.001's
HTTPS-only transport restriction and REQ-0011.004's HTTPS-only package-location
restriction. All other version-1 schema, selection, cache, package, signature,
and offline behavior remains in force. Existing HTTPS configuration migrates as
HTTPS with HTTP permission false. No source locator is rewritten during
migration.

## Security Consequences

Filesystem permissions, SMB authentication, TLS, and removable-media ownership
provide access properties but not package or index authorization. HTTP exposes
locator, metadata, and availability to observation and modification; signatures
and atomic cache validation limit integrity impact but do not provide
confidentiality or freshness.

## Explicit Non-Goals

- WPM does not mount media, discover a moved drive, or manage SMB credentials.
- The read interface does not author, synchronize, or repair a repository.
- HTTP opt-in does not suppress warnings or establish trust.
- This decision does not change the version-1 `index.json` or package format.
- Automatic mirror failover may not substitute an unapproved origin or stale
  identity after plan approval.

## Consequences

Repository consumers depend on one narrow reader and typed locator parser.
Deterministic adapter tests cover most failures, while native SMB, removable
media, optical media, proxy, and architecture evidence remain release-matrix
obligations. Transport-specific errors become actionable without leaking
credentials or corrupting prior cache state.

## References

- REQ-0011, REQ-0012, REQ-0013, and REQ-0016
- Planned TC-0016
- ADR-0001, ADR-0002, and ADR-0005
- `docs/dfs.md`

