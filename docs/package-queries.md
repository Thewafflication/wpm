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

## Available catalog inventory (1.2.30)

`wpm list --available [package] [--arch <arch>] [--version <semver>]` reads
configured filesystem catalogs and existing web caches, following bounded linked
roots without preparing caches or fetching archives. The same exact selectors
apply. All matching versions remain visible, including duplicates across roots.
Rows show repository ordinal, priority, linked parent where relevant, and stable
or prerelease-allowed/excluded policy. Policy labels do not certify archive trust
or successful installation; no candidate is silently selected by this inventory.

Rows sort by name, architecture, descending semantic version, full version
string, priority, root ordinal, and locator tie-break. The existing global and
package prerelease settings are read without changes. Source locators are
omitted; `repo diagnose --links` explains source availability and graph issues.
Verbose query internals also avoid printing repository locators.

Catalog reads share the bounded grammar/schema/document reader with author
verification/publication. Required entry fields, semantic versions, supported
architectures, and existing transport/relative-path rules are checked. Invalid
entries are skipped and counted while other usable entries remain visible.
Catalogs with malformed or duplicate required root fields are rejected. This
strict query does not change the legacy install resolver's parsing/defaults.

Missing, unreadable, invalid, or stale catalogs, invalid configuration/links,
invalid entries, and exhausted bounds yield an explicit partial result and exit
1. Stale cache entries remain visible and labeled. No matches in a complete
healthy query returns 0. Bounds remain 32 roots, 96 catalog entries, and 4 MiB per
index. No data/cache/trust/log writes, scripts, web retrieval, extraction, or
archive integrity checks occur. Filesystem/UNC reads use the caller's OS access.
The result describes present local/cache metadata, not the latest remote state.

The loopback fixture records zero requests and checks full filesystem snapshots.
It covers linked roots/cycles, duplicate versions, semantic order, exact filters,
prerelease policy, absent data/log roots, stale/missing/locked catalogs, invalid
entries/root fields/links/configuration, credentials, root/entry bounds, and
stable complete/partial status. Detailed show/selection, trust/deployment health,
and general machine output remain open in the broader I-08 work.

## Combined package metadata (1.2.31)

`wpm show <package> [--arch <arch>] [--version <semver>] [--offline]` combines
the shared retained-record and local/cached linked-catalog queries. It shows all
matching versions, architectures, and source choices, along with catalog
priority/prerelease labels. Multiple matches prompt selector refinement instead
of silently choosing a candidate. The same metadata-only interpretation and
read-only/no-web-retrieval guarantees apply.

Exit status is 0 only when both queries are complete and at least one record
matches. A complete healthy query with no matches reports no matching metadata
and returns 1. An incomplete query reports that absence cannot be concluded from
the inspected sources; readable matches remain visible with `complete: no`.
Malformed/missing selectors and inappropriate flags fail before initialization.

The catalog fixture covers combined retained/catalog metadata, exact selectors,
duplicate choices, unknown packages under healthy and incomplete sources,
verbose/offline placement, zero loopback requests, full snapshots, and scripts
that never run. Detailed archive trust/integrity, deployed state, selected
candidate plans, and general machine output remain open in the broader I-08 work.
