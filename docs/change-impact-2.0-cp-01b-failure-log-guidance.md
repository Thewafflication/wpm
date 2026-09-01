# WPM 2.0 CP-01B Failure-Log Guidance Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Point command failures to the active operational log

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

After operational logging initializes, the first WPM `Error:` event prints the
exact active log path. The guidance is emitted at most once per process to avoid
obscuring the primary diagnostic when a failure produces multiple errors. The
guidance itself is retained in the log.

Errors rejected before durable initialization remain narrow and do not claim a log
exists. Package, repository, signature, installation, and audit formats are unchanged.

## Risk and verification

The risks are misleading guidance when no log exists, repeated noise, and divergence
between default and custom destinations. TC-0032 proves a failing operation reports
its custom path exactly once, retains the pointer, and an early invalid invocation
neither initializes state nor prints log guidance.
