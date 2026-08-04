# REQ-0014: Command Output and Help

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 1

## Scope

Applies to every public WPM command, global option, interactive prompt,
package-script boundary, and operational diagnostic on supported Windows
targets. It extends, but does not replace, the invocation and verbose-output
rules in REQ-0001 and the command-specific output in REQ-0002 through
REQ-0013.

## Requirement

**REQ-0014.001**
WPM shall provide task-oriented help for every public command. The help shall
include a short, valid example for initialization, build, verification,
installation, removal, update, upgrade, signing-key selection, trust
management, repository management, configuration, diagnosis, and recovery as
those commands apply.

**REQ-0014.002**
WPM shall use consistent semantic presentations for progress, success,
warning, error, prompt, package-script output, and final result summaries.
Each mutating command shall finish with an unambiguous result for every
selected package identity or operation.

**REQ-0014.003**
WPM shall accept `--color auto|always|never`. `auto` shall emit styling only
when the applicable output stream is connected to an interactive terminal;
`always` shall emit the documented styling; and `never` shall emit no styling.
The styles shall distinguish progress, success, warning, error, prompts, and
package-script output without relying on color alone. Redirected and
machine-consumed output shall contain no terminal-control sequences under
`auto`.

**REQ-0014.004**
WPM shall report concise progress for repository access, local or network
copy, download, archive creation or extraction, validation, staging, and
package-script execution. Interactive progress may update in place, but
non-interactive progress shall remain line-oriented, bounded, and stable.
Normal output shall not report per-file detail unless that detail is required
to explain an error.

**REQ-0014.005**
For every mutating command, `--verbose` shall add the selected repository or
source, resolved package identity, cache and staging locations, validation
phases, script invocation and result, retained artifacts, and final operation
result. Verbose output shall add operational detail rather than repeat normal
messages and shall not expose private keys, credentials, environment secrets,
or avoidable protected data.

**REQ-0014.006**
WPM shall write structured, timestamped operational logging at a documented
location. Log verbosity shall be configurable without suppressing required
failure evidence, and a command failure shall identify the relevant log path
when a log is available. Logs shall preserve line boundaries and severity and
shall not contain terminal-control sequences or secrets. A stricter identified
no-mutation requirement, including REQ-0015 dry run, shall suppress persistent
logging and report that no durable log was created.

**REQ-0014.007**
An invalid command, option, operand, or option value shall produce a nonzero
status, identify the invalid input, and display the narrowest relevant usage
form and next useful help command. It shall not print unrelated full help in a
way that hides the diagnostic.

**REQ-0014.008**
Package-script output shall be presented in a clearly delimited section that
identifies the package and phase while preserving the script's standard
output and standard error content. Delimiters and WPM messages shall remain
distinguishable in redirected output, and WPM shall not reinterpret
package-script text as its own success or failure result.

## Rationale

Consistent, accessible, and scriptable output makes package operations easier
to understand and makes failures actionable without weakening unattended or
CI usage. A shared semantic contract also prevents command-specific output
from drifting as new 2.0 commands are added.

## Verification

**Method:** Automated test, inspection, and demonstration

**References:** Planned TC-0014; `docs/traceability-2.0.md`

Planned verification covers every public help path, redirected and interactive
output, color selection, semantic classes, lifecycle progress, verbose secret
redaction, operational-log structure and configuration, invalid arguments,
and bounded package-script output.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 1 and the WSP logging and
  visual-style adoption recorded in `docs/wsp-adoption.md`; governed by
  ADR-0010.
- **Depends on:** REQ-0001 through REQ-0013 command behavior, WSP-SEC-0010,
  WSP-TEST-0005, and the accepted logging boundary in `docs/dfs.md`.
- **Conflicts with:** None. Existing command-specific output remains valid
  where it satisfies this shared contract.

## Change Impact

This requirement affects the public CLI contract, console/TTY detection,
logging, every command dispatcher, package-script presentation, usage
documentation, and output-sensitive tests. It introduces no package, archive,
signature, repository-index, or installed-record format change. Compatibility
risk is concentrated in scripts that parse human-oriented output; stable
machine output is specified separately by REQ-0018.

## Tailoring

None. Applicability changes require the normal WSP adoption and
requirement-change process.

## Implementation Record

Planned allocation is a shared presentation/logging layer used by command,
archive, repository, signing, trust, initialization, and upgrade code; TC-0014
will provide command-level automated verification. ADR-0010 fixes the event,
renderer, redaction, and machine-envelope boundaries. No 2.0 runtime
implementation or verification evidence is claimed by this proposed baseline.
