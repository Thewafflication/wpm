# WPM 2.0 CP-01B Verbose-Redaction Verification Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Verify URL credentials remain undisclosed in verbose output

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

WPM rejects repository URLs containing user information before persisting or
using them. TC-0025 already proved rejection and normal-output non-disclosure,
but did not enable the diagnostic mode governed by REQ-0014.005. This
verification increment exercises the same adversarial locator with
`--verbose`, proves verbose diagnostics are active, and rejects any output
containing either the complete user-information pair or its password suffix.

No executable behavior changes. This is objective evidence for one sensitive
input class; it does not claim that REQ-0014.005 is complete across every
mutating command or every possible secret source.

## Risk and verification

The principal test risk is a false pass caused by verbose mode not taking
effect. The assertion therefore requires a `Verbose:` diagnostic in addition
to the nonzero exit status and the absence of `user:secret` and `secret@`.
TC-0025 retains its existing repository policy, progress, trust, origin,
package-install, and UNC coverage.
