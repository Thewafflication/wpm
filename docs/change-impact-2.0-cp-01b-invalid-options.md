# WPM 2.0 CP-01B Invalid-Option Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Validate command options before durable initialization

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

This increment centralizes the accepted option names and value requirements for
all public commands. Unknown options and missing option values now fail before
WPM initializes its data directories or operational log. Diagnostics identify
the command and option, show one command-specific usage line, and direct the
user to `wpm help <command>` instead of printing unrelated full help.

The same usage-line renderer supplies command-specific help, preventing the
narrow diagnostic and help syntax from drifting. Negative numeric repository
priorities remain valid values. Package, repository, archive, signature, and
installed-state formats are unchanged.

## Risk and verification

The principal risks are treating an option as a package/path operand, rejecting
a supported option, consuming a missing value from the following option, and
creating state before validation. TC-0028 checks every public command, every
value-taking option family, narrow output, and the no-initialization boundary.
Existing command workflow tests continue to cover accepted options. Operand
arity/value semantics beyond option recognition remain pending in TC-0014.

Full-suite verification initially exposed that compacting global `--verbose`
also removed the private self-upgrade completion marker before its handoff
parser read it. The final implementation exempts only
`--complete-self-upgrade` from that public-command compaction; TC-0013 then
passed with verbose propagation through both self-upgrade stages.
