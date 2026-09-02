# WPM 2.0 CP-01B Result-Summary Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add consistent identity-qualified final results to install and
remove

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

`upgrade` already emits `Result: <name> <arch> <status>` for each selected
package, but direct `install` and `remove` end with command-specific prose.
This increment adds the same stable final-result shape to successful install
and remove operations while preserving their existing human-readable success
messages and all preceding progress and package-script output.

Install uses the metadata already validated from the selected package. Remove
reads the retained archive's validated package metadata before invoking its
remove script and does not claim an architecture-less result if that identity
cannot be established. Failure paths do not emit a successful final result.
No invocation syntax, package/archive format, retained state, script behavior,
or exit status changes.

## Risk and verification

The principal risks are duplicate results, a summary that is not actually
final, identity drift between the operated package and summary, or removal of
an unreadable retained package. TC-0034 builds one isolated package, installs
and removes it, and requires each command's final nonempty line to be exactly
one `Result: <name> <arch> installed|removed` record. Existing install,
upgrade, removal, and package-script tests retain their prior output and state
assertions. REQ-0014.002 remains Planned overall until every mutating command
has equivalent final-result evidence.
