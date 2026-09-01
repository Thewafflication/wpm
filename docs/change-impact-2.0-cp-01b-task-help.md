# WPM 2.0 CP-01B Task-Help Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add task-oriented help for every public command

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

This increment adds `wpm help <command>` and `wpm <command> --help` without
initializing WPM data directories or operational logging. Every public command
receives a narrow usage form, task description, and short example. General help
now covers initialization, build, verification, installation, removal,
repository refresh, upgrade, signing-key generation and selection, trust,
configuration, diagnosis, and removal/reinstallation recovery.

The previously special-cased `keygen` dispatcher is assigned a normal command
identifier so its help follows the same path; its execution syntax and behavior
are unchanged. Package and repository formats are unaffected.

## Risk and verification

The risks are accidental mutation during help, divergence between the two help
forms, an incomplete command inventory, and full-help noise on invalid topics.
TC-0027 exercises both forms for all twelve public commands, checks the required
common-task inventory, proves the isolated data root remains absent, and checks
the narrow unknown-topic diagnostic. Remaining CP-01B progress, verbose, and
failure-log work stays allocated to TC-0014.
