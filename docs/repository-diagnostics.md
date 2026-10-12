# Read-only repository diagnosis (1.2.21)

Run `wpm repo diagnose` to inspect configured repository catalog health without
refreshing indexes or downloading packages. `--offline` is accepted as an
explicit reminder; the command always reads web catalogs from existing caches. Filesystem sources
use the caller's OS access, including UNC shares. `--verbose`
works before or after the command. No WPM data directories, caches, operational
logs, trust records, package state, scripts, or handoffs are created or changed.

The report follows configured-root order (including the built-in repository)
and checks local source indexes or existing web caches against the same JSON
and schema-admission checks used by refresh. It identifies unavailable sources,
invalid or unreadable indexes, stale web caches, and invalid configuration.
HTTP roots are labeled as insecure. Repository numbers can be matched to
`wpm repo list`; diagnostic output omits locators to avoid credential exposure.

Exit status is 0 when every loaded root is usable and web caches are fresh;
1 indicates attention or an invalid invocation. Remediation points to source
media/read access, `wpm update`, and configuration repair. Blank/comment lines
are ignored. Unknown flags, extra operands, priority changes, and HTTP opt-in
flags are rejected before initialization. Existing `--diagnose` behavior and
all existing commands retain their defaults.

`tests/repository-health-integration.py` snapshots all filesystem contents and
modification times, checks absent and existing data roots, healthy and corrupt
catalogs, stale caches, disconnected sources, invalid configuration, verbose
output, credential redaction, external log paths, and invalid options. A real
loopback server records requests and must receive none.

This is an additive repository-health slice. Linked-root health, installed
records, architecture conflicts, pending self-upgrade, trust assessment, and a
versioned machine-readable report remain allocated to broader I-06/I-08 work.
I-04/I-05 media and managed-network evidence also remains open. This delivery
does not claim those parent increments complete; breaking changes stay on 2.x.

## Linked catalog inspection (1.2.22)

`wpm repo diagnose --links` also inspects reachable linked catalogs using local
source files and existing web caches. It never prepares missing caches. The
report keeps configured roots first, appends linked roots in discovery order,
and identifies each linked root's parent repository number. Missing or invalid
catalogs stop discovery through that root; other readable branches continue.

The query shares resolution's root normalization, transport restrictions,
cycle/duplicate handling, and 32-root bound. It reports link issues for unsafe
references, malformed link fields, or exhausted capacity and returns attention
status. Locators remain omitted, including rejected credential-bearing links.
The default report and existing public diagnosis function retain their behavior.

The snapshot/loopback fixture covers nested local roots, cached web cycles,
duplicates, missing/stale/corrupt descendants, unsafe references, capacity,
credential safety, strict flag scope, and zero managed filesystem changes or web requests.
Installed records, architecture conflicts, pending self-upgrade, trust
assessment, and versioned machine output remain future work.
