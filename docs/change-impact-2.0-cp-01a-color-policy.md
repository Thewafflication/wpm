# WPM 2.0 CP-01A Color-Policy Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add the first shared human-presentation contract slice

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

This increment implements the `--color auto|always|never` decision at the
existing shared console/logging adapter. It styles recognized semantic labels,
uses native console attributes on interactive Windows consoles, emits ANSI only
when `always` is explicitly selected for a redirected stream, and passes plain
message bytes to operational logging. The global option is removed before
command parsing, so it may appear before or after the command; when repeated,
the last value wins.

Invalid color values and unknown commands now fail with an identified input,
narrow usage, and a help direction. Existing package, repository, archive,
signature, and installed-state formats are unchanged.

## Risk and verification

The principal risks are terminal-control bytes in redirected auto output,
styling that removes semantic meaning, command operands accidentally consuming
the global option, and logging styled bytes. The renderer retains textual
labels, decides styling only after stream inspection, removes the option before
dispatch, and logs the unstyled formatted message. TC-0026 covers the automated
redirected decision table, precedence, option position, invalid values, narrow
unknown-command output, and help. Genuine-console inspection and the remaining
semantic/event migration stay allocated to TC-0014; CP-01A is therefore in
progress rather than claimed complete.
