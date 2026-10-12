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

## Escaped JSON strings (1.2.16)

Index field names, package strings, and linked roots now decode standard JSON
escapes, including Unicode escapes and paired UTF-16 surrogates converted to
UTF-8. Decoded paths undergo the existing containment checks; escaped traversal
cannot introduce a linked root. Invalid escapes, unpaired surrogates, embedded
NUL escapes, and unescaped control characters are rejected by string decoding.
This does not change the supported Windows filesystem path encoding.
The loopback integration test covers escaped field names and links, unknown
Unicode fields, escaped traversal, and malformed escape sequences.

## Cache publication admission (1.2.17)

Refresh retrieves an index into a temporary candidate, checks supported
schema-1 root metadata and document completeness, then atomically publishes it.
Unsupported schema, missing catalog arrays, unterminated documents, and failed
reads retain the previous cache. This admission check is not a replacement for
a complete JSON/schema validator. Web repositories retain their established
cached fallback; invalid local sources fail closed. Rejected candidates are
removed without changing prior cache bytes or modification time, and rejected
links are never followed. The loopback and local-media integration fixtures
verify these behaviors and absence of leftover candidate files.

## JSON grammar validation (1.2.18)

Refreshed, cached, and directly consumed local indexes share JSON grammar
validation before catalog entries or linked roots are used. Invalid delimiters,
commas, literals, number forms, control characters, raw NUL bytes, and trailing
data are rejected. Prior web caches survive rejected refreshes; malformed local
dry-run sources fail without writes. Existing legal extension values, long
unknown keys, numeric schema-1 forms, and escaped strings remain supported.

The grammar state machine is iterative and allocates its state buffer from the
already bounded input length (4 MiB maximum), avoiding native call-stack depth
limits. Integration fixtures include 4,096 nested arrays, long extension keys,
legal Boolean/null/number/string values, a negative grammar corpus, cache-byte
and timestamp preservation, and complete dry-run filesystem snapshots. These
checks validate JSON grammar, not every future package/index schema obligation
or the Windows filesystem encoding contract.

## Exact schema numbers (1.2.25)

Schema admission compares the decimal JSON number exactly with 1. Decimal
values just above or below 1 cannot become supported versions through floating
point rounding. Equivalent forms such as `1.0`, `10e-1`, and `0.1e1` remain
supported, including long zero mantissas and exponents within the input bound.
Rejected refreshes preserve cache bytes and timestamps; direct local diagnosis
reports unsupported versions without writes.
