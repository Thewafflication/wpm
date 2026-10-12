# Retained installed-package listing (1.2.29)

`wpm list [package] [--arch <arch>] [--version <semver>] [--offline]` reads
metadata from retained archives beneath the active data root's packages store.
It reports name, architecture, version, and archive filename. Package and
architecture selectors are exact and case-insensitive; version selectors are
exact. No matches is a successful empty query.

Rows sort by name and architecture, then descending semantic version. Build
metadata ties sort by the full version string and archive filename. Multiple
retained versions and architectures remain visible; no record is silently
selected as proof of the deployed version. No extraction or package scripts
run, and no initialization, data/cache/trust/log writes, or web retrieval occur.
Filesystem/UNC reads use the caller's Windows access.

This is a metadata inventory. It does not verify full archive integrity,
signature trust, or the current externally deployed software. Do not infer that
a listed record can safely be installed or replayed. Use the existing archive
verification and package-maintainer recovery workflow where appropriate.

An absent store or no archives returns 0. A complete readable query also returns
0. Unreadable/invalid metadata, unsupported layout, or a limited query returns 1
with readable records and an explicit summary. Metadata must have a supported
architecture and semantic version. The query examines at most 256 matching
entries; a limited result is a bounded subset, not a complete history. Reparse
directories/files are rejected. Read handles deny writes/deletion while each
record is inspected, and terminal/credential filtering applies to output.

The integration fixture creates a signed install and additional retained
versions/architectures, checks semantic ordering and exact selectors, and uses
full filesystem snapshots to verify absent data/log roots and zero mutations.
Corrupt/locked records, credential-bearing invalid metadata, count bounds, and
an actual NTFS package-store junction are covered. Script markers remain absent.

This is a partial I-08/CP-05B delivery. Available catalogs, detailed show/selection
queries, full integrity/trust assessment, conflicts, upgrade/recovery context,
and general machine output remain open. Existing operation defaults and package
formats remain compatible; breaking changes stay reserved for 2.x.
