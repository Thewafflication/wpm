# Recorded upgrade failure inspection (1.2.28)

`wpm recovery [list] [--limit <1..4096>] [--offline]` lists existing `*.upgrade-failed.txt` records
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
