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
path or command line. Without `--archive`, inspection validates a recognized controlled recovery
copy derived from the current data root and operation ID. Older external
references remain unchecked and non-success. An explicit current archive is validated
in place against the recorded BLAKE2b-256 digest, package identity/version,
full archive integrity, and present signature/trust policy. Revoked or unknown
signers remain unauthorized; `--allow-unsigned` is explicit and retains the
existing data-root/privilege policy and may also validate the controlled copy
without --archive. A missing removal version in an older
record remains unknown instead of being inferred from current software.

Inspection performs no extraction, retrieval, logging initialization, script,
handoff, trust change, cleanup, or persistent write. Directory ancestors are
guarded against rename and checked for reparse points. Records and current inputs
open relative to held directory handles, and archive identity/integrity/trust
checks read the same immutable handle. JSON enumeration also uses the held
directory. Sharing does not prevent every in-place reparse change; checks before
and after admitted I/O fail unsafe state closed, with no pathname fallback.
Files must also be ordinary single-link files; locks, unsafe namespaces, malformed data,
or identity changes fail closed. This slice supports canonical local-drive
paths; UNC/device record roots and current-input paths are not admitted.

## Schemas and results

The retained `wpm.recovery.v1` document carries operation/type, UTC millisecond
timestamps, approved plan digest, status/phase, package identity and old/new
versions, redacted source, input path/digest, signer (`unsigned` or public key
ID), recorded validation, external-state uncertainty, and observed script exit. An approved input is copied from its held handle
into a fresh operation-specific root before staging or scripts, then hashed,
flushed, and held immutable through execution. Its relative reference is
<operation-id>/input.zip, independent of mutable package metadata. IDs and
retained paths are bound into the plan digest. Known completed control phases
are ordered; failed or unobserved work is never inferred as completed.
Unknown values remain null. Initial record failure blocks planned package
mutation. Atomic updates preserve the last valid document, and failed temporary
evidence remains retained. Scheduled self-upgrades remain pending.

Temporary records remain exclusively open through same-parent native rename
after their bytes are flushed. The XP-compatible Windows adapter checks native
structure layouts at compile time and fails closed when an API is unavailable.

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
inspect only. Known recovery-input roles are interpreted; unsupported roles remain
explicitly uninterpreted and cannot yield complete inspection. Retained input
status/path are separate from an explicit archive status, so a good supplied
archive cannot hide a missing or changed recorded copy. Complete staging/log/
handoff inventories remain open.

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

REQ-0019 and composite TC-0019 remain open. Complete phase/artifact inventories
(including cache preparation, staging, logs, and self-upgrade completion),
newly confirmed retry, conservative cleanup, restore, and controlled
native/fault/media evidence are subsequent work.
There is no general rollback guarantee for package-script effects.


Planned installs retain package records under their validated canonical
name-architecture-version.zip identity. This prevents an opaque recovery-copy
filename or a caller's generic alias from colliding with another package's
record. The displayed plan names that destination. Ordinary 1.x execution
preserves its legacy filename/default behavior; archive contents and repository
formats are unchanged. Retained evidence is not automatically removed here.
