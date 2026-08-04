# ADR-0010: Command Events and Machine-Readable Output

**Status:** Accepted

**Date:** 2026-08-04

**Relationship:** Supplements ADR-0007 and the logging boundary in
`docs/dfs.md`; it does not supersede any accepted 1.x command behavior.

## Context

WPM 2.0 adds consistent human presentation, operational logging, and stable
machine-readable results. If commands format each destination independently,
selection semantics, redaction, severity, and final status can diverge. Raw
terminal text is also unsafe as a logging or serialization interface.

## Decision Drivers

- Human output must remain accessible and useful in terminals and redirection.
- Automation needs one versioned, deterministic contract.
- Logs and renderers must share status without sharing secrets or terminal bytes.
- Package-script text must not be mistaken for WPM's own result.
- Existing 1.x human-readable output is not a stable parser contract.

## Considered Options

1. Typed command events with independent human, JSON, and log renderers.
2. Parse human console text to produce logs and JSON.
3. Let each command define unrelated text and machine schemas.
4. Expose the internal event stream as the public machine protocol.

## Decision

WPM SHALL create typed command events above the console and logging layers.
Each event contains a semantic class, stable event code, operation and phase,
safe object identity, structured fields, sensitivity classification, and final
status when applicable. Human text is a renderer concern and is not parsed to
recover those fields.

Progress, success, warning, error, prompt, result, and package-script boundary
events use this contract. Package-script stdout and stderr remain attributed
opaque text inside explicit package and phase boundaries; they cannot supply a
WPM event code or final result.

Before dispatch to any renderer, fields SHALL be classified as public,
support-sensitive, or secret. Secret fields are never rendered. Credentials,
URL user information, tokens, private-key material, and avoidable environment
values are redacted at the event boundary rather than independently by each
destination.

## Human Presentation

Human mode is the default. Styling is applied only by the human renderer after
TTY policy is resolved. Semantic labels, delimiters, and final status remain
meaningful without color. `--color auto` writes no control sequences to a
redirected stream; log and machine renderers never receive control sequences.

Normal human output may evolve for clarity. It remains line-oriented when not
interactive and is not a supported field-level automation interface. Stable
event codes may be included in diagnostics without freezing prose.

## Machine Contract

The public machine mode is `--output json`. For every handled command outcome
it writes exactly one UTF-8 JSON document followed by one line-feed to standard
output. The top-level envelope contains:

- `schema`, initially `wpm.output.v1`;
- `command` and normalized, non-secret selectors;
- `status` and `exitCode`;
- `result`, whose command-specific schema is identified within the envelope;
- `diagnostics`, as ordered structured event records; and
- required fields with JSON `null` when the schema defines an unavailable value.

Arrays use documented deterministic sort keys. Object members are emitted in a
documented order for reproducible fixtures, although consumers SHALL treat
member order as insignificant. New optional members may be added within a
schema version only when old consumers can ignore them; removal, type changes,
meaning changes, or a new required interpretation require a new schema value.

Machine mode emits no color, progress animation, prompts, package-script
delimiters, or human prose. A command that would require confirmation fails
before mutation unless an explicit non-interactive authorization such as
`--yes` is present. Handled warnings and errors are represented in the JSON
envelope, keeping standard error empty. Failures before the serializer can
establish an envelope may use standard error and a nonzero process status.

## Logging Boundary

The operational logger consumes the same already-redacted events but has its
own level and destination policy. It records event codes, severity, phase, and
safe fields; it does not copy styling bytes or raw secret-bearing arguments.
Raw package-script output is bounded and attributed, and is recorded only at
the configured level after the same protected-data policy is applied.

## Compatibility and Migration

No package, archive, repository-index, signature, installed-record, or 1.x
command-input format changes. Existing human output remains available, but
scripts that scrape it should migrate to `--output json`. `wpm.output.v1` is a
new 2.0 contract and has no 1.x data migration.

## Security Consequences

Central event classification reduces inconsistent redaction and terminal-byte
injection. Structured output remains attacker-influenced data, so serializers
must escape strings, bound fields, reject invalid encoding, and never construct
JSON by concatenating untrusted text. Stable event codes disclose no additional
secret context.

## Explicit Non-Goals

- Human wording, spacing, and table layout are not a stable API.
- This decision does not expose an unbounded streaming telemetry protocol.
- It does not localize event codes or allow package scripts to emit trusted WPM
  events.
- It does not make color, TTY state, or log availability a trust signal.
- It does not add machine mode to mutating commands beyond an identified
  requirement.

## Consequences

A shared event model and serializers become dependencies of public commands.
Tests must compare human and machine selection semantics, validate schemas and
redaction, and exercise terminal, redirected, serialization-failure, and log
paths. The additional abstraction is justified by one security and consistency
boundary instead of repeated formatting logic.

## References

- REQ-0014 and planned TC-0014
- REQ-0018 and planned TC-0018
- ADR-0007
- `docs/dfs.md`

