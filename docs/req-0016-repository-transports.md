# REQ-0016: Repositories

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 3

## Scope

Applies to repository configuration, refresh, package acquisition, diagnostics,
and audit behavior for local filesystems, removable and optical media, UNC/SMB,
HTTPS, and explicitly enabled HTTP repositories.

## Requirement

**REQ-0016.001**
WPM shall use one transport-neutral logical repository contract for supported
repository locators while preserving the version-1 `index.json`, package
archive, signature, trust, selection, cache, and offline rules established by
REQ-0011 through REQ-0013.

**REQ-0016.002**
`wpm repo add`, `repo list`, `repo update`, `update`, and named installation
shall support absolute local directory paths, drive roots, fixed disks,
USB/removable media, and CD-ROM or other read-only filesystem media. A
read-only source shall remain consumable without WPM attempting to modify it.

**REQ-0016.003**
The repository commands in REQ-0016.002 shall support UNC paths to SMB shares.
WPM shall use the caller's authorized operating-system access and shall not
persist or print share credentials. Authentication failure, disconnection,
timeout, and partial read shall identify the affected locator and phase without
weakening validation or corrupting an earlier usable cache.

**REQ-0016.004**
The repository commands in REQ-0016.002 shall support plain HTTP only when
`wpm repo add <http-url> --allow-insecure-http` records an explicit Boolean
permission scoped to that canonical repository. HTTP shall be disabled by
default and shall have no global enable switch; use shall display a clear
transport-security warning in interactive and line-oriented output and shall
record the insecure transport in diagnostics and audit data. Opt-in shall not
authorize another origin, HTTPS downgrade, or redirects to otherwise disallowed
schemes.

**REQ-0016.005**
Local, SMB, HTTP, and HTTPS repositories shall use the same index and package
signature validation and local trust decisions. Repository location, media
ownership, filesystem permissions, network authentication, HTTP opt-in, or
successful transport shall not cause WPM to trust a package, index, or signing
key.

**REQ-0016.006**
Configuration, normal and verbose output, diagnostics, operation plans, and
audit/recovery records shall preserve enough of the configured and effective
repository locator to distinguish network, local, and removable sources.
Credentials, URL user information, tokens, and protected share data shall be
redacted.

**REQ-0016.007**
WPM shall detect and report repository media that is absent at operation start,
changes drive letter, becomes unavailable between phases, changes identity or
content after planning, or returns an incomplete read. WPM shall fail the
affected operation closed, retain prior valid package state and caches, and
identify a safe retry action; it shall not silently substitute a different
volume or repository.

**REQ-0016.008**
Repository locator parsing and path resolution shall reject unsupported or
ambiguous schemes, relative repository roots unless explicitly permitted by a
future requirement, traversal outside the selected root, device-path confusion,
and index entries that escape the logical repository. Local paths that contain
URL-significant characters shall be treated according to their selected
locator type rather than by substring heuristics.

## Rationale

Offline and managed local-network distribution require the same signed
repository to work from disks, removable media, optical media, and shares.
Legacy systems may require HTTP, but explicit opt-in and unchanged signature
validation keep transport risk separate from package authorization.

## Verification

**Method:** Automated test, negative test, fault injection, inspection, and
managed-environment demonstration

**References:** Planned TC-0016; `docs/traceability-2.0.md`

Planned verification covers writable and read-only local repositories,
removable-media loss and replacement, UNC/SMB success and failure, HTTP opt-in
and warnings, HTTPS regression, cache preservation, unsafe locators, and valid
and invalid signatures from every supported transport. Native SMB and optical
media evidence may supplement deterministic isolated tests at the release gate.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 3 and ADR-0005's common
  logical repository structure; governed by ADR-0011.
- **Depends on:** REQ-0011, REQ-0012, REQ-0013, REQ-0014, REQ-0015,
  WSP-ROB-0001, WSP-SEC-0003, WSP-SEC-0006, and WSP-SEC-0009.
- **Supersedes for WPM 2.0:** REQ-0011.001's HTTPS-only repository restriction
  and REQ-0011.004's HTTPS-only package-location restriction. REQ-0011 remains
  the unchanged accepted 1.x contract; its version-1 schema, HTTPS validation,
  cache, priority, selection, package-validation, and offline rules continue to
  apply where REQ-0016 does not explicitly extend transport behavior.
- **Conflicts with:** None after the scoped 2.0 supersession above and ADR-0011.

## Change Impact

This requirement changes repository locator parsing, source configuration,
transport acquisition, cache identity, diagnostics, audit records, and the
repository trust boundary. It requires ADR and DFS review before
implementation. The repository layout and signed package formats remain
unchanged. Platform testing must distinguish deterministic transport adapters
from real SMB, removable-media, and optical-media behavior.

## Tailoring

None for the declared 2.0 repository matrix. A platform-specific test
substitution requires an approved test-strategy equivalence and residual-risk
record; it does not remove the supported behavior.

## Implementation Record

Planned allocation is a typed repository-locator parser and narrow
transport-neutral reader with filesystem, UNC/SMB, HTTPS, and opted-in HTTP
adapters. Existing repository selection, cache, signature, and trust logic is
to remain shared above the adapter boundary. TC-0016 will provide transport,
fault, and trust evidence. ADR-0011 excludes SCP from the required 2.0 provider
set and prohibits automatic media relocation or redirect downgrade. No
implementation is claimed by this proposed baseline.
