# Recorded upgrade failure inspection (1.2.28)

`wpm recovery [list] [--limit <1..4096>] [--json] [--offline]` lists existing `*.upgrade-failed.txt` records
in the active data root's audit directory. It reports the audit filename,
package/architecture, old/new versions, recorded phase/exit code, and archive
reference. Descending filename order places ordinary generated timestamps
newest first. The query does not assert that an archive reference still exists.

The command performs no managed writes, initialization, logging, web retrieval,
extraction, scripts, trust changes, retry, or cleanup. Verbose placement is
supported. Filesystem/UNC reads use the caller's Windows access. An absent audit
directory or no matching records is a successful empty query. Exit status is
0 for a complete query, even when failed operations are listed. It is 1 for
invalid/unreadable records, a partial bounded query, invalid arguments, or an
inaccessible/unsupported layout.

The reader checks required field completeness, duplicates, failed status,
architecture, archive leaf references, NULs, field capacities, and 32-bit exit
codes. Each record is at most 16 KiB; by default at most 128 entries are examined.
The optional `--limit` accepts decimal values from 1 to 4096, including
leading zeros. It bounds examined entries, including invalid records; it does
not select the newest N records across a larger directory. A bounded subset
is sorted after enumeration. Raise the limit to inspect a larger directory;
exceeding the selected limit remains a partial query with exit status 1. Invalid
records are counted without raw contents; exceeding the bound is explicit.
Directory/record reparse points are rejected. Credential/terminal filtering
applies to displayed text. Read handles deny writes/deletion during parsing.

These text records are mutable and unsigned. They describe a recorded attempt,
not authenticated current installation state or proof of archive trust. Inspect
the named audit and retained source/archive with the package maintainer before
retrying; revalidate integrity, trust, and current state. A failed script may
already have changed deployed software. The query never infers safe replay
from an exit code or audit entry alone.

The integration fixture creates a signed install and attempts a new version
whose install script exits with code 7. It checks the recorded phase/code and
old/new identity. Full filesystem snapshots prove queries do not mutate audits,
logs, cache, trust, or script markers. Other cases cover absent data/external
log roots, strict CLI rejection, no-final-newline records, malformed/duplicate
fields, exit-code overflow, invalid status/archive references, NUL/oversize
input, credentials, real unreadable locking, bounds, and an NTFS audit junction.

This is a partial I-06 delivery. Interrupted installs, pending handoffs, failed
audit writes, authenticated recovery state, guarded retry/cleanup, backup/restore,
and physical environment evidence remain open. Existing operation defaults and
audit formats remain compatible. Breaking changes stay reserved for 2.x.

## JSON audit query (1.2.34)

`wpm recovery list --json [--limit <1..4096>]` renders the same bounded records
as one UTF-8 JSON document with a terminating newline. Color policy has no
effect. It emits no human prose, script output, prompts, operational logs, or
standard-error diagnostics for a validly formed query. Invalid CLI arguments
retain the existing human usage errors; `--verbose` cannot combine with `--json`.

The schema is `wpm.recovery-list.v1`, a compatible optional audit query contract.
It does not implement the broader planned `wpm.output.v1` envelope or recovery
record format. Required members have these meanings:

| Member | Meaning |
| --- | --- |
| `schema` | Always `wpm.recovery-list.v1` |
| `records` | Same descending filename order and inspected subset as human mode |
| `summary.records` | Parsed records emitted |
| `summary.invalid_or_unreadable` | Matching entries rejected or unreadable |
| `summary.limited` | Enumeration exceeded the selected bound |
| `summary.limit` | Selected examined-entry bound |
| `summary.serialization_issues` | Text fields unavailable for JSON encoding |
| `status` | `complete`, `partial`, or `unavailable` |
| `complete` | Complete enumeration, parsing, and serialization |
| `exit_code` | 0 when complete; otherwise 1, matching the process status |
| `authenticated` | Always false: existing audits are mutable and unsigned |
| `external_state_known` | Always false: script effects are not assessed |
| `automatic_retry_supported` | Always false: this query never replays scripts |

Each record has string fields `record` (audit filename), `name`, `arch`,
`old_version`, `new_version`, `archive_reference`, and `phase`, plus numeric
`exit_code` for the recorded DWORD script status. References do not assert
artifact presence, current deployed identity, or trust. Unknown audit extensions
remain ignored. Consumers must tolerate new optional members; incompatible
field meanings or types require a new schema.

Text uses the shared human-event credential and terminal filter before JSON
escaping. Quotes, backslashes, and controls are escaped. Valid UTF-8, including
supplementary characters, is retained. Invalid UTF-8 or allocation failure in a
text field emits `null`, increments serialization issues, and returns a partial
result. No replacement decoding or locale conversion changes existing audit
bytes. Legacy native-encoded records remain available through human mode.

Empty or absent directories are complete with an empty array. Unsupported or
inaccessible directory layouts are unavailable with an empty array. Parsed
records remain visible when others are invalid or enumeration is limited.
An output I/O failure returns nonzero; a broken destination cannot be guaranteed
to receive a complete document. Human mode and audit formats remain unchanged.

The real failed-upgrade fixture checks human/JSON identity and exit status,
empty/unavailable/partial queries, bounded ordering, Unicode, escaping, invalid
UTF-8 forms, credential filtering, forced color, no writes, and no script replay.
