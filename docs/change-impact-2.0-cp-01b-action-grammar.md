# WPM 2.0 CP-01B Action-Grammar Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Validate nested actions and reconcile task examples

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

This increment validates repo, key, trust, and config action/operand combinations
before durable initialization. Invalid forms receive one relevant usage line and
command-help direction. It also corrects two help examples discovered during
reconciliation: trust-add takes one public-key path, while package-scoped
prerelease configuration takes the Boolean before `--package`.

No persistent format or accepted command syntax changes. The validator centralizes
the grammar already implemented by each command handler so malformed input fails
before configuration, logging, or other durable state is initialized.

## Risk and verification

The principal risk is rejecting a valid existing invocation while moving grammar
checks ahead of command dispatch. TC-0030 covers invalid partitions, corrected
examples, narrow recovery guidance, and the no-state boundary. TC-0012 and TC-0013
cover accepted trust and config mutations; TC-0024 and TC-0025 cover accepted local,
HTTPS, UNC, and opted-in HTTP repository workflows; TC-0027 through TC-0029 cover
the adjacent help, option, and simple-operand validation layers.
