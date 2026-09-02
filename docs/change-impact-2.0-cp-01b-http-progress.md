# WPM 2.0 CP-01B HTTP-Progress Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Present known HTTP response length in redirected progress

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

The opted-in HTTP repository adapter already reads and enforces the response
`Content-Length`, but passes zero to the shared progress renderer. Redirected
HTTP output therefore reports only bytes received even when a trustworthy
total is available. This increment passes the parsed response length through
progress start and update calls so redirected output includes percentage,
current bytes, and total bytes. A response without `Content-Length` retains
the existing unknown-total presentation.

No transport permission, redirect, origin, repository, cache, package trust,
or incomplete-response decision changes. The final received-length comparison
remains authoritative; progress presentation does not relax validation.

## Risk and verification

The principal risks are displaying a total different from the value enforced
after transfer or making redirected output noisy. TC-0025's isolated Python
loopback server supplies a deterministic nonzero `Content-Length`. The refresh
step requires the loopback repository's update record to be immediately
preceded by one stable zero-percent start and one byte-qualified completion
with the same total. Scoping the assertion to that repository is important
because developer and CI configurations can also refresh built-in HTTPS
repositories in the same command. Existing HTTP policy, warning, origin,
credential, trust, package-install, and UNC assertions remain unchanged.
REQ-0014.004 remains Planned overall until every lifecycle phase and bounded
long-operation output has objective evidence.
