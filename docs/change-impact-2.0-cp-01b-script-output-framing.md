# WPM 2.0 CP-01B Package-Script Output Framing Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Identify the package in package-script output delimiters

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

Package-script output is already presented in a delimited section that names the
script phase. This change extends the opening and closing delimiters to also name
the package, so the section identifies both the package and phase required by
REQ-0014.008. The delimiters become `--- package <name>: <phase> script output ---`
and `--- end package <name>: <phase> script output (exit code <n>) ---`.

The script's standard output and standard error remain interleaved and unaltered
inside the section, the retained per-script log is unchanged, and WPM continues to
derive success or failure only from the script's exit code, never from the script's
printed text. This applies uniformly to install, upgrade-install, and removal
scripts.

Package, repository, signature, index, installation, upgrade, and audit formats are
unchanged. The only user-visible change is the additional package identity in the two
delimiter lines; TC-0004's delimiter assertion is updated to the package-qualified
form.

## Risk and verification

The risks are that a package script could emit WPM-looking success or failure text
and be mistaken for WPM's own result, and that redirected output could blur the
boundary between WPM messages and script output. TC-0033 builds one package whose
install script exits zero while printing `Error:` and `Result:` lines on both
standard output and standard error, and one package whose install script prints a
false success and exits nonzero. It confirms the delimiters name the package and
phase, both script streams are preserved, the WPM-looking lines are contained within
the delimited section, the zero-exit install still succeeds, and the nonzero-exit
install fails with its exit code framed.
