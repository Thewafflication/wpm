# Read-only machine output

`--output json` emits the ADR-0010 `wpm.output.v1` envelope for these read-only
queries: `--diagnose`, `list`, `list --available`, `show <package>`,
`repo diagnose [--links]`, and `recovery [list]`. It adds no network retrieval,
package extraction, scripts, operational logging, or persistent initialization.
Legacy `--json` remains available with its original schema and fields.

Examples:

```text
wpm --diagnose --output json
wpm list --available --arch x64 --output json
wpm show example --version 1.0.0 --output json
wpm repo diagnose --links --output json
wpm recovery list --name example --output json
```

## Envelope contract

One UTF-8 JSON document followed by LF is written to stdout. Handled errors
leave stderr empty. Member order is schema, command, selectors, resultSchema, result,
diagnostics, serializationComplete, status, exitCode. Consumers must ignore
object member ordering and tolerate new optional members. Nested result
whitespace is insignificant.

- `schema` is `wpm.output.v1`.
- `command` is diagnose, list, show, repo, or recovery; unsupported/missing
  commands use null. The `repo` command currently permits diagnosis only.
- `selectors` always contains name, arch, version, repository, available,
  links, and limit in that order. Absent string/numeric selectors use null;
  booleans default false. Architecture is lowercase; version retains the
  validated exact selector. Repository is the displayed ordinal (1..32).
  Recovery's examined-entry limit defaults to 128; other commands use null.
- `resultSchema` identifies the command-specific contract, or is null when no
  query is executed. Repository health uses wpm.repository-health.v1 to name
  its unchanged legacy version: 1 contract, whose nested document has no schema
  string.
- `result` nests the existing command-specific document, including its schema
  (`wpm.runtime-diagnose.v1`, `wpm.package-query.v1`,
  `wpm.repository-health.v1`, or `wpm.recovery-list.v1`). Invalid arguments,
  unsupported commands, or unencodable selectors produce null before querying.
  Nesting does not change existing query completeness, selection, trust, or
  metadata-only limitations. Package queries do not prove deployed state or
  archive signature trust; runtime diagnosis is not the complete 2.0 health
  report. Inspect nested schema/status/complete fields before using rows.
- `diagnostics` is an ordered array of typed, already-redacted events with
  severity, code, operation, phase, and message fields. Usage events are retained
  as info records; query failure supplies `output.query.incomplete`.
- `serializationComplete` is false if envelope text cannot be encoded or
  diagnostic allocation/collection exceeds its bound. At most 32 diagnostics
  are retained; a bound failure cannot return success. Nested serializers retain
  their existing null/serialization issue rules and nonzero statuses.
- `status` is success for a successful query, partial for an executed query
  returning nonzero (including no match/unavailable data), or error if no query
  result was produced. This status is not an installation or trust claim.
- `exitCode` equals process status: 0 on success, otherwise 1. Output I/O failure
  returns nonzero; a broken destination may not receive a complete document.

`--output json` or `--output=json` may precede or follow the command. Offline
and valid color policies are accepted; color never affects machine output.
Diagnosis also accepts dry-run. Other inspection commands retain their existing
rejection of dry-run. Duplicate selectors and output modes, verbose, help,
legacy --json, invalid values, and mutating commands fail before mutation.
Normalized selector text uses the same credential/terminal filter and UTF-8
encoder as result fields. Invalid encoding emits null and non-success rather
than replacement decoding. No raw command arguments or inherited environment
are serialized.

## Verification and acceptance

`tests/machine-output-integration.py` compares nested documents to legacy
queries, checks populated retained/catalog fixtures, exact ordering and nulls,
normalized selectors, repeated-byte determinism, handled invalid/unsupported
shapes, redaction, bounded diagnostics, operational failures, and before/after
byte/mtime/attribute/root snapshots, and a real broken output pipe that must
return nonzero without stderr or writes. Final output is explicitly flushed.
CTest runs this foundation in native
x86/x64/ARM64 verification and full suites.

This is partial REQ-0018.008/.009 evidence. Full 2.0 health, remediation,
resolver eligibility/trust/upgrade/recovery context, ambiguity semantics and
composite profile acceptance remain open. Requirements are not marked complete
merely because the common envelope exists.
