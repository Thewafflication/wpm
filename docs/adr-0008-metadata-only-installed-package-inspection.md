# ADR-0008: Metadata-Only Installed Package Inspection

Status: Accepted

## Context

WPM retains installed package archives beneath its data directory. Commands such
as `wpm update` and `wpm upgrade` enumerate those archives to determine each
installed package's name, version, and architecture.

The original inspection implementation extracted every archive into a temporary
directory and then read `.wpm/package.txt`. This made a metadata query proportional
to the total uncompressed size of every installed package. Large payload files
caused unnecessary disk I/O, temporary storage use, antivirus scanning, verbose
output, and visible delays during routine update checks.

Package inspection and package verification have different purposes. Inspection
identifies an installed package for version selection. Verification establishes
the integrity and authenticity of an archive before deployment.

## Decision

WPM SHALL read `.wpm/package.txt` directly from an installed package ZIP when it
needs only the package name, version, and architecture.

Metadata-only inspection SHALL:

- locate `.wpm/package.txt` in the ZIP central directory;
- decompress only that entry into bounded memory;
- apply the same metadata syntax and safe-value validation used for package
  metadata read from a directory;
- avoid creating an inspection staging directory; and
- avoid extracting package payload files.

The metadata entry read for inspection SHALL be limited to 1 MiB. A missing,
oversized, unreadable, or invalid metadata entry SHALL make the installed package
record unreadable for update and upgrade selection.

Metadata-only inspection SHALL NOT replace full archive extraction, package-index
validation, or signature validation when installing, upgrading, or explicitly
verifying a package. Those operations SHALL continue to authenticate the complete
package before executing package scripts.

## Rationale

An update availability check requires only a small, known metadata entry. Reading
that entry directly makes inspection proportional to metadata size instead of
payload size and avoids writing untrusted package contents to disk merely to learn
the installed identity.

Keeping inspection separate from verification preserves the package trust model:
inspection metadata is sufficient for comparing installed and repository
versions, but it is not treated as proof that the retained archive is authentic
or complete.

The retained archive remains the installed-state record in this design. A separate
installed-package database could make lookup still faster, but would introduce a
second state store that must be synchronized and recovered. Direct ZIP metadata
access resolves the immediate performance problem without that consistency cost.

## Consequences

### Positive

- Update and upgrade planning no longer extracts installed payloads.
- Inspection time and temporary storage use are independent of payload size.
- Antivirus and filesystem activity during routine update checks are reduced.
- Interrupted inspection no longer leaves `inspect-*` staging directories.
- Installed-state authority remains with the retained package archive.

### Negative

- ZIP metadata must be readable before an installed package can participate in
  update or upgrade selection.
- Inspection intentionally does not detect corruption in payload entries; full
  verification remains a separate operation.
- The metadata parser is used across both directory-backed and archive-backed
  reads and must remain behaviorally consistent.

