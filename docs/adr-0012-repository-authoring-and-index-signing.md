# ADR-0012: Repository Authoring and Index Signing

**Status:** Accepted

**Date:** 2026-08-04

**Relationship:** Supplements ADR-0002, ADR-0005, and ADR-0011. It defines a
separate local writer and repository-index signer authorization; it does not
make a repository a package-signing trust anchor.

## Context

The accepted repository design defines a logical version-1 tree and a detached
`index.sig`, but it does not define deterministic authoring, atomic replacement,
or how an index signer is authorized. WPM 2.0 requires WPM itself to initialize,
populate, index, sign, verify, copy, and consume repositories, including on
read-only distribution media.

## Decision Drivers

- Untrusted archives must not escape or corrupt an authoring root.
- Output must be deterministic and independent of authoring host paths.
- Index authority must be explicit and separate from package-signing authority.
- Private keys must not enter repositories, logs, plans, or evidence.
- Existing unsigned 1.x HTTPS repositories need an explicit compatibility path.
- Verification must work without modifying read-only media or local state.

## Considered Options

1. A local-filesystem writer sharing validation with the transport-neutral reader.
2. Give every repository transport write capability.
3. Trust any key distributed in a repository's `keys` directory.
4. Require a new repository schema for authoring.

## Decision

Repository authoring SHALL use a local-filesystem-only writer that is separate
from ADR-0011's reader. It accepts one canonical, allow-listed root; rejects
device paths and escaping or reparse-point traversal; inventories conflicts;
and uses the operation-plan and dry-run boundary in ADR-0013. Network and media
publication is an external complete-tree copy followed by read-only WPM
verification, not a writable repository-provider capability.

`repo init` creates only the approved version-1 layout and non-authoritative
local guidance. `repo add-package` fully validates an archive before creating a
sibling temporary file, flushing it, revalidating identity and destination,
and atomically renaming it into place. Existing content is not overwritten
unless the computed plan names the exact replacement and confirmation permits
it. Partial files never use a valid package filename.

`repo index` derives entries only from validated archives, emits relative
forward-slash package paths, and sorts by normalized package name, architecture,
SemVer, and path. It emits the existing version-1 schema without absolute paths,
drive letters, credentials, staging paths, or an implicit current timestamp.
Given identical archives, policy, and explicit metadata, the index bytes are
identical. The old index is atomically replaced only after the complete candidate
and optional signature validate.

`repo verify` uses the same readers and validators as consumption but performs
no cache, trust, audit, or repository writes. Missing indexed packages,
unindexed package archives, duplicate identities, path escapes, invalid
signatures, and entry/archive disagreement are errors. Documented public keys
and authoring guidance outside `packages/` are reported but are not package
content or trust anchors; other unexpected files are findings under the
selected verification policy.

## Repository-Index Signing Authority

An authored `index.sig` is a detached Ed25519 signature over the exact
`index.json` bytes and records the supported algorithm and durable key ID.
Index-signing authorization is repository-scoped configuration established by
an explicit administrator action through an independent channel. It is a
separate role from package-signing authorization even when the same public key
material is intentionally approved for both roles.

A key present in repository content, a valid signature by an unpinned key, a
TLS certificate, or a trusted package-signing role does not authorize an index
signer. A repository configured to require a pinned index signer fails closed
when the signature is absent, changed, unsupported, or made by another key.
Removing `index.sig` cannot downgrade that policy.

The author selects a private key explicitly for one signing operation. WPM does
not copy or retain it, clears secret buffers, and renders only the public key ID.
Dry-run validates public selection and destination policy without opening or
using private-key material.

## Compatibility and Migration

The version-1 `index.json` and package formats are unchanged. Existing 1.x
repository entries migrate as legacy package-authenticated sources with no
pinned index signer and a visible diagnostic until an administrator explicitly
configures index-signing policy. This compatibility mode does not treat an
unverified index as package authority; package signature, manifest, and local
trust checks remain mandatory. Newly signed repositories can be consumed by
1.x implementations because the signature is a detached sidecar.

## Security Consequences

Index signing authenticates catalog integrity and limits malicious mirror and
media substitution after explicit pinning. It does not prevent an authorized
index signer from hiding or replaying packages, and it does not authorize any
package signer. Authoring has destructive filesystem potential, so root
containment, reparse rejection, atomic files, no-follow behavior where available,
and pre-commit identity checks are mandatory.

## Explicit Non-Goals

- WPM does not create optical filesystems, mount media, or synchronize mirrors.
- Repository index signing does not replace package signatures or local package
  trust.
- WPM does not discover, escrow, rotate, or publish private authoring keys.
- Authoring does not rewrite a package or repair attacker-controlled archives.
- This decision does not add location-specific fields to version-1 metadata.

## Consequences

Authoring reuses archive, metadata, signature, and repository readers but owns a
narrow local mutation module. Repository configuration gains an additive,
repository-scoped index-signer policy; legacy entries remain readable under a
documented lower-assurance mode. Tests must cover determinism, atomic failure,
unsafe roots, signer roles, downgrade, read-only verification, and
create-copy-consume journeys.

## References

- REQ-0012, REQ-0015, REQ-0016, and REQ-0017
- Planned TC-0017
- ADR-0001, ADR-0002, ADR-0005, ADR-0011, and ADR-0013
- `docs/dfs.md`

