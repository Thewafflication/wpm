# WPM 2.0 CP-01B Verbose-Operation Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Complete verbose install and removal lifecycle details

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

Install already reports its archive, staging directory, validation activity,
script invocation, and retained archive in verbose mode, but does not state the
resolved metadata identity explicitly. Removal previously relied mainly on
low-level extraction and script diagnostics and did not identify its retained
archive, removal staging area, resolved metadata identity, or archive deletion.

This increment adds those operation-specific verbose records. Normal progress,
package-script framing, final results, exit status, package format, validation,
and state transitions are unchanged. The messages contain paths and package
metadata already required for diagnosing the selected operation; WPM does not
inspect or emit unrelated environment values.

## Risk and verification

The principal risks are logging the requested identity before validated
metadata is available, claiming archive deletion before it occurs, duplicating
normal output, or exposing unrelated environment state. TC-0035 builds and
installs an isolated package, then removes it. It requires the relevant
identity, staging, validation, script, retention/deletion, and final-result
records in each partition and proves a unique environment-secret sentinel is
absent. Identity is logged only after metadata validation; deletion is logged
immediately before the attempted retained-archive transition. REQ-0014.005
remains Planned until the remaining mutating commands and secret classes have
equivalent evidence.
