# REQ-0017: Repository Authoring

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 4

## Scope

Applies to creation, population, indexing, signing, verification, and copying
of version-1 WPM repositories on local filesystems.

## Requirement

**REQ-0017.001**
`wpm repo init <directory>` shall create the controlled repository layout and
local authoring guidance in a new or safely reusable directory. It shall not
overwrite an existing repository, package, index, signature, or unrelated
content without an approved plan and confirmation, and repeated safe
invocation shall be deterministic.

**REQ-0017.002**
`wpm repo add-package <repository> <archive>` shall validate archive structure,
metadata, path containment, package index integrity, package signature and
trust policy, identity conflicts, available space, and repository writability
before copying. The destination update shall be atomic at repository-file
granularity and shall not leave a valid-looking partial package after failure.

**REQ-0017.003**
`wpm repo index <repository>` shall deterministically build or refresh the
version-1 `index.json` from validated repository archives. It shall reject
duplicate or ambiguous identities, invalid versions or architectures, unsafe
paths, archive/index disagreement, and entries outside the repository root,
and it shall atomically replace an earlier index only after complete success.

**REQ-0017.004**
`wpm repo index <repository>` shall support signing the resulting index with a
configured maintainer key under the approved repository-signing design. Key
selection, cryptographic validation, secret handling, signature identity,
failure, and audit behavior shall follow REQ-0012 and the DFS. Repository-index
signer authorization shall be repository-scoped and separate from package-
signer authorization under ADR-0012; private key material shall not enter
repository content, normal output, logs, or retained test artifacts.

**REQ-0017.005**
`wpm repo verify <repository>` shall perform read-only validation of repository
layout, index schema and signature, duplicate entries, entry/archive identity
agreement, package availability, package index and signature validity, path
containment, and the documented policy for unindexed files. It shall report
every independently discoverable finding and return nonzero if any required
property fails.

**REQ-0017.006**
Authoring mutations shall use the operation plan, confirmation, dry-run,
progress, verbose, logging, and result contracts in REQ-0014 and REQ-0015.
Verification shall not require or create mutable repository, cache, trust, or
audit state.

**REQ-0017.007**
A repository generated on writable local storage shall work unchanged after
the complete repository tree is copied to removable or optical-mastering
media and made read-only. Generated repository content shall contain no
absolute authoring path, drive letter, source locator, credential, staging
path, or other host-specific dependency.

**REQ-0017.008**
WPM documentation shall define a repeatable authoring journey that initializes
a repository, adds packages, builds and signs its index, verifies it, copies
the complete tree to USB or an optical-mastering directory, verifies the copy,
configures it as a source, updates its index cache, and installs a package.

## Rationale

Built-in authoring makes the signed repository contract usable without a
separate unpublished toolchain and ensures that offline media is produced by
the same validators used to consume it.

## Verification

**Method:** Automated test, negative test, inspection, and demonstration

**References:** Planned TC-0017; `docs/traceability-2.0.md`

Planned verification covers initialization conflicts, corrupt and duplicate
packages, insufficient space, read-only targets, deterministic indexes,
interrupted atomic replacement, valid and invalid signing keys, tampering,
read-only verification, and a create-copy-consume round trip.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 4.
- **Governed by:** ADR-0012's local writer and index-signer authorization, and
  ADR-0013's plan/dry-run boundary.
- **Depends on:** REQ-0003, REQ-0005, REQ-0012, REQ-0014, REQ-0015,
  REQ-0016, ADR-0002, ADR-0005, and WSP-SEC-0008.
- **Conflicts with:** None identified. Repository index signing requires a new
  repository-scoped signer policy. ADR-0012 preserves legacy unsigned 1.x
  consumption as an explicit lower-assurance compatibility mode while keeping
  package validation mandatory.

## Change Impact

This requirement adds public repository-authoring commands and new local
filesystem mutation paths. It affects signing-key use, repository validation,
atomic file replacement, documentation, and release-media journeys. Security
review must address unsafe roots, reparse points, overwrite races, signing-key
disclosure, and attacker-controlled archives. It does not change package
archive or version-1 index schemas unless separately approved.

## Tailoring

None. Optical-media creation may use an external mastering utility, but WPM
shall create and verify the directory tree that the utility consumes.

## Implementation Record

Planned allocation is a repository-authoring module layered over existing
archive, metadata, signing, and transport-neutral repository validation. The
CLI dispatcher will expose four subcommands, and TC-0017 will provide atomicity,
signing, validation, and removable-copy evidence. ADR-0012 keeps the writer
local-only and generated version-1 indexes deterministic and locator-neutral.
No implementation is claimed by this proposed baseline.
