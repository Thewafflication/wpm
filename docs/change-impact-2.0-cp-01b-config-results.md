# WPM 2.0 CP-01B Configuration-Result Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add final results to prerelease configuration mutations

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

Successful prerelease `config set` and package-scoped `config unset` commands
previously ended with descriptive prose. This increment adds one stable final
record identifying the global or package scope and, for set, the effective
Boolean value. Configuration grammar, precedence, persistence, lookup, and
failure behavior are unchanged.

## Risk and verification

The principal risks are confusing global and package scope or reporting a
value different from the persisted mutation. TC-0037 applies global true,
package false, and package removal transitions in an isolated data root and
requires exactly one matching result to be the final nonempty line after each.
REQ-0014.002 remains Planned pending remaining mutating commands.
