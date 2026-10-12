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

## Versioned JSON report (1.2.28)

`wpm repo diagnose --json [--links] [--offline]` emits one JSON document on
standard output for a valid invocation, including unhealthy configurations.
The exit status is still 0 for healthy and 1 when attention is required. The
existing human report remains the default. `--verbose` is rejected with JSON
before initialization; forced color does not add terminal escapes to JSON.

The report has integer `version: 1`, Boolean `follow_links` and `healthy`, a
`roots` array, and a `summary` object. Each root provides `ordinal`, Boolean
`configured`, `parent` (0 for a configured root), `transport` (`filesystem`,
`http`, or `https`), Boolean `insecure`, and `status` (`valid`, `stale`, `invalid`,
or `unavailable`). Ordinals follow the same configured/discovery order as the
human report. The built-in root counts as configured.

Summary keys are `configured_roots`, `inspected_roots`, `usable_indexes`,
`unavailable`, `invalid`, `stale`, `invalid_configuration`, and `link_issues`.
A stale index is usable but requires attention. If configuration paths cannot
be resolved, roots are empty, invalid configuration is 1, and an additional
`error: "configuration_unreadable"` identifies the failure. Consumers should
check the version and tolerate additional fields in future compatible reports.

As of 1.2.29, diagnosis also counts configuration entries omitted after the
32-root capacity is reached. It continues checking remaining configuration
lines, ignores blank/comment lines, and reports excess or malformed entries
as invalid configuration. It does not inspect more than 32 roots. Normal
resolution retains its established capacity and selection behavior.

Locators, credentials, filesystem paths, and raw exception text are omitted.
The command retains the same read-only/no-web-retrieval guarantees and linked
graph bounds. Argument errors retain normal CLI usage diagnostics; they are
not JSON health reports. For example:

```text
wpm repo diagnose --json --links > repository-health.json
```

The health integration fixture parses actual output with a JSON parser, checks
counts/status/parent relationships, absence of credentials and locators, forced
color, configuration-resolution failures, CLI rejection, filesystem snapshots,
and a loopback server that receives no requests. Broader installed-state health
and general machine output remain open in I-06/I-08; breaking changes stay on 2.x.
