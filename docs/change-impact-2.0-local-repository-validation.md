# WPM 2.0 Local-Repository Validation Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Reject malformed drive-qualified repository paths and isolate
invalid persisted repository entries

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

WPM 1.2.2 can accept a filesystem repository path with a second colon after
the drive prefix, such as `C:\C:`. It also trusts persisted repository locators
without applying the same validation used by `repo add`. An invalid entry is
therefore repeatedly treated as a filesystem source during `update`, producing
a misleading read failure for a path that can never name a Windows directory.

This change rejects a colon after the drive prefix during repository-path
normalization. Configuration loading quietly validates each persisted locator,
uses only its canonical value, and warns while skipping invalid entries. The
warning identifies invalid filesystem locators and gives a bounded removal
command. Web-locator rejection remains generic so malformed credentials cannot
be echoed. `repo remove` accepts an exact invalid filesystem entry as a repair
path while continuing to reject malformed web locators and control characters.

No valid local, UNC, HTTP, or HTTPS locator; repository schema; package trust
decision; cache identity; or package format changes.

## Risk and verification

The principal risks are rejecting a valid Windows path, reinterpreting a
relative persisted entry against a later working directory, or making corrupt
configuration impossible to repair. TC-0024 retains its absolute, relative,
space-containing, read-only, traversal, file-URL, device, trust, and removal
coverage. It additionally rejects `C:\C:`, injects that exact persisted entry,
proves update ignores it instead of attempting I/O, and removes it through the
public CLI. Relative persisted filesystem entries are rejected rather than
re-resolved because only `repo add` is authorized to resolve relative roots.
