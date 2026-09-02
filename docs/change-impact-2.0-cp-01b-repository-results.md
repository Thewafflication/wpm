# WPM 2.0 CP-01B Repository-Result Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add final result summaries to repository mutations

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

Repository add, reprioritization, removal, and refresh report descriptive
success output but previously had no stable final result. This increment adds
one final record after each completed operation. Add and reprioritization use
`Result: repository configured <canonical-locator>`, removal uses
`Result: repository removed <canonical-locator>`, and refresh uses
`Result: repositories updated` after update reporting completes.

The result is emitted only after the configuration rewrite, cache cleanup, or
repository refresh/reporting path succeeds. Repository selection, canonical
locator rules, priority, HTTP permission, cache behavior, update availability,
trust policy, and failure output are unchanged.

## Risk and verification

The principal risks are claiming success before all mutation steps complete,
leaking a noncanonical locator, or allowing later informational output to make
the result ambiguous. TC-0034 adds, refreshes, and removes an isolated local
repository and requires exactly one expected result to be the final nonempty
line for each operation. Its existing package install/remove result assertions
remain in place. Existing local, HTTP, HTTPS, and upgrade tests retain their
behavioral coverage. REQ-0014.002 remains Planned until the other mutating
commands have equivalent evidence.
