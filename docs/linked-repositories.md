# Linked repository layout (1.x)

Schema version 1 supports an optional `repositories` array of repository roots:

```json
{
  "version": 1,
  "packages": [
    {"name": "example", "version": "1.2.12", "arch": "any",
     "url": "example-any-1.2.12.zip"}
  ],
  "repositories": [
    "https://example.org/releases/1.2.11"
  ]
}
```

Each root supplies its own `index.json`; its package URLs resolve against that
root. A release can therefore link to its predecessor without copying old
archives or catalog entries. Release automation links to the highest earlier
published stable version with an index asset. Older clients ignore this optional
field and continue consuming the current release's packages.

Local roots may link to contained relative subdirectories (for example
`previous`). HTTPS roots may link to HTTPS roots or contained relative roots.
Explicitly enabled HTTP roots retain their same-origin HTTP restriction. Links
cannot introduce a protocol downgrade, an absolute local path, or `..` traversal.
Package signatures and trust decisions remain independent of repository links.

Linked roots inherit the parent priority and are appended after configured roots.
Roots are deduplicated, including cycles, before access. The graph is bounded by
32 total roots and 96 package entries; capacity exhaustion emits a warning.
A missing linked index leaves readable roots available. `wpm update` refreshes every discovered linked web index, even inside the
normal cache freshness window; offline resolution uses cached web indexes. Dry-run and
confirmation execution never retrieve missing indexes during read-only planning.

## Verification

The dry-run integration test selects an older version from a relative linked
root, then installs and removes it through the normal lifecycle. Complete
filesystem snapshots verify dry-run preservation. Fixtures also cover duplicate
links, unsafe traversal, cached HTTPS cycles and forbidden HTTP downgrades.
The release pipeline performs native x86, x64 and ARM64 verification and an
upgrade from the previous stable release before publishing.

This is an additive 1.x delivery. Broader transport work in I-04/I-05 remains
open; breaking defaults or schemas stay reserved for 2.x.

## Explicit refresh verification (1.2.14)

`tests/linked-repository-integration.py` exercises a real loopback HTTP server
with repository-scoped opt-in. It verifies nested root refresh during both
`update` and `repo update`, fresh-cache reuse for ordinary resolution, no
requests for offline/dry-run resolution, cycles and duplicate roots, failed
linked refresh fallback, and the bounded 32-root graph. Unsafe traversal and
cross-origin HTTP links are rejected. The fixture disables the built-in HTTPS
backend to keep its network traffic confined to the loopback server.

This compatible fix makes an explicit update visit each root once; previously
linked roots could remain stale until their normal freshness period expired.
Package verification and trust rules are unchanged.

## Index schema recognition (1.2.15)

Schema recognition now reads the top-level `version` field, accepting JSON
whitespace and a UTF-8 BOM. Nested package/extension versions, strings, or
version 10 cannot accidentally authorize schema 1. The catalog must provide a
top-level `packages` array. Existing schema-1 numeric representations, unknown
extension fields, package ordering, and optional links remain supported.
The linked-repository integration test covers these forms using real index
retrieval and verifies that unsupported catalogs cannot introduce linked roots.
