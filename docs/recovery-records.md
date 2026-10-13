# Recovery record inspection

Explicitly planned lifecycle operations (`--confirm`) retain additive
`wpm.recovery.v1` records under `<WPM data root>\recovery`. Ordinary 1.x
execution and the existing `recovery list` failure-audit contract are unchanged.
Recovery documents describe WPM-controlled history and recorded uncertainty;
they are unauthenticated and cannot establish deployed software state or safe
retry of arbitrary package scripts.

```text
wpm recovery records
wpm recovery records --name example --arch x64 --output json
wpm recovery inspect <operation-id>
wpm recovery inspect <operation-id> --archive .\current-package.zip
wpm recovery inspect <operation-id> --archive .\unsigned.zip --allow-unsigned
```

`records` enumerates bounded diagnostic identities in operation-ID order. The
default examined-entry limit is 128; `--limit` accepts 1..4096. Invalid or
unreadable records and an exhausted bound make the whole query incomplete,
including when a name or architecture filter hides other records. Enumeration
does not dereference stored archive paths or assert current archive validity.

`inspect` requires one lowercase 32-digit hexadecimal operation ID, never a
path or command line. Without `--archive`, an external input remains unchecked
and inspection returns non-success. An explicit current archive is validated
in place against the recorded BLAKE2b-256 digest, package identity/version,
full archive integrity, and present signature/trust policy. Revoked or unknown
signers remain unauthorized; `--allow-unsigned` is explicit and retains the
existing data-root/privilege policy. A missing removal version in an older
record remains unknown instead of being inferred from current software.

Inspection performs no extraction, retrieval, logging initialization, script,
handoff, trust change, cleanup, or persistent write. Directory ancestors are
guarded against rename and checked for reparse points. Records/current inputs
must be ordinary single-link files; locks, unsafe namespaces, malformed data,
or identity changes fail closed. This slice supports canonical local-drive
paths; UNC/device record roots and current-input paths are not admitted.

## Schemas and results

The retained `wpm.recovery.v1` document carries operation/type, UTC millisecond
timestamps, approved plan digest, status/phase, package identity and old/new
versions, redacted source, input path/digest, signer (`unsigned` or public key
ID), recorded validation, external-state uncertainty, and observed script exit.
Unknown values remain null. Initial record failure blocks planned package
mutation. Atomic updates preserve the last valid document, and failed temporary
evidence remains retained. Scheduled self-upgrades remain pending.

`--json` emits `wpm.recovery-query.v1`; `--output json` nests the same result in
`wpm.output.v1`. The envelope adds record, archive, and allowUnsigned selectors
only for these new queries. Records use deterministic field order and shared
UTF-8/credential filtering. Source documents are bounded at 64 KiB and cumulative reads at 64 MiB;
required
identity fields are unique (including escaped duplicate keys), timestamps and
schema admission are validated, and future/malformed records are preserved.

Each row reports `inputStatus`: not-checked, external-unchecked,
validated-current-input, missing, changed, identity-incomplete,
identity-conflict, signer-conflict, unsafe-path, unreadable, invalid, or
untrusted-or-invalid. A validated current input does not authenticate the
record or authorize another operation. `authenticated` and
`externalStateKnown` remain false; `retryEligible` is false and safeActions is
inspect only. Phase/artifact inventories are not fully interpreted yet;
`artifactInventoryInterpreted` is explicitly false.

The result includes examined/returned/invalid/filtered/limited counts,
complete, exitCode, and retrySupported (false). Exit 0 means a complete
enumeration or successful explicit current-input inspection, not recovery or
rollback success. Exit 1 means invalid selection, incomplete enumeration, or
unverified/current-invalid input. Human and JSON render the same admitted
identities. Handled common-machine errors retain typed envelope diagnostics.

## Verification and remaining acceptance

The registered recovery-record integration exercises real failed/completed
operations, no-write byte/time/attribute snapshots, both JSON modes, filters,
malformed/future/duplicate/tampered records, explicit archive changes, current
revocation, locks, hard links, and reparse roots. Removal records retain their
known old version; earlier null-version records are inspectable as incomplete.
The existing lifecycle, query, and repository regressions cover shared parser
and directory-guard refactoring.

REQ-0019 and composite TC-0019 remain open. Retained input copies and complete
ordered phase/artifact inventories, automatic controlled-root input checking,
newly confirmed retry, conservative cleanup, restore, self-upgrade completion
linkage, and controlled native/fault/media evidence are subsequent work.
There is no general rollback guarantee for package-script effects.
