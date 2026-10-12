# WPM data, removal, backup, and restore (1.2.35)

This guide describes compatible 1.x behavior and quiescent whole-state restore
expectations. It adds no automatic cleanup, retry, or rollback command.

## Locate the installation and active data

Run `wpm --diagnose` to inspect the current executable/runtime and data root.
An explicit `WPM_DATA_DIR` selects the data root; otherwise WPM uses
`%ProgramData%\WPM`, with `C:\ProgramData\WPM` as the fallback when that
environment variable is absent. Record an absolute root and the account/scope
that selects it. Relative overrides depend on the caller's working directory.

`setup.cmd --user` normally installs binaries under `%LocalAppData%\WPM` and
sets the user data root to `%LocalAppData%\WPM\data`. Machine setup installs
binaries under `%ProgramW6432%\WPM` or `%ProgramFiles%\WPM`; its default data
root is separate. Explicit installation/data overrides can change or overlap
these locations. Copying a portable executable alone does not select a new
mutable data root; use an explicit `WPM_DATA_DIR` for isolated portable state.

| State | Role and backup expectation |
| --- | --- |
| Installation directory | WPM executable, runtime, and distribution documents; retain the matching release separately |
| `packages` | Retained archives used for identity and package removal; include in the whole-state backup |
| `config` | Repository/prerelease settings and optional default signing-key reference; include |
| `trust` | Public trust records and revocation markers; preserve together and reconcile current revocations before reuse |
| `cache` | Repository/package inputs and self-upgrade handoff files; preserve pending inputs and provenance |
| `temp` | WPM staging/verification content; classify active or interrupted work before treating it as disposable |
| `audit` | Upgrade history and failure evidence; preserve, including unsuccessful attempts |
| `logs` | Operational/script evidence when stored here; preserve required incident history |
| External state | Deployed software, script-created files/registry values, external logs, sources, and signing keys need separate backup |

An archive inventory describes retained metadata, not proof of current deployed
software or archive trust. A default signing-key configuration refers to a
private key by path; it does not embed that key in the data root. Back up needed
external private keys separately with appropriate access protection. Repository
configuration and logs may also contain support-sensitive information. Preserve
the intended account/ACL protections rather than making a restored root broadly
writable. Ordinary file-copy metadata does not establish equivalent Windows ACLs.

## Package removal and WPM product removal

`wpm remove <archive-name>` targets one retained package archive. It stages and
verifies the archive, runs its package removal script, and deletes the retained
archive only after success. Package-specific scripts determine external removal
effects. Preview an individual package operation with `--dry-run` or request its
existing optional `--confirm` plan before changing it.

The distribution's `remove.cmd [--user|--machine]` has a different scope: it
recursively deletes the selected installation directory and selected data root,
including retained archives, configuration, trust, cache/staging, audits, and
logs inside those roots. This is the existing 1.x default; there is no supported
`--keep-data` or dry-run option for that script. Confirm the scope and absolute
`WPM_INSTALL_DIR`/`WPM_DATA_DIR` overrides before invoking it, especially when
the user data directory lies inside the installation directory. Save required
state outside both deletion roots first.

Product removal updates the selected persistent WPM/Path environment entries
and removes the user data-root entry for user scope. It does not run every
package's removal script or undo arbitrary package effects outside those roots.
External logs, authored repositories, private keys, and deployed applications
outside the selected roots remain separate lifecycle responsibilities. A backup
of WPM records is not a backup or uninstall of all managed software.

## Create a quiescent whole-state backup

1. Record WPM release/architecture, the absolute data and installation roots,
   scope/account, environment configuration, external repository/signing paths,
   and separately deployed software that must be restored together.
2. Finish or stop WPM operations, package scripts, authoring, and background
   self-upgrade handoffs. A returned parent prompt alone does not prove a
   scheduled self-upgrade has finished. Review its completion log and retained
   failure evidence. Do not capture or replay a live handoff as a ready restore.
3. Copy the complete data root to a fresh destination outside the installation
   and data roots using an administrator-approved backup tool. Preserve file
   contents, hierarchy, relevant timestamps, permissions, and revocation markers.
   Preserve external logs, keys, sources, deployment state, and matching binaries
   separately. Avoid following unexpected junctions or links into other data.
4. Check the copy against the source with a file inventory and hashes, record
   omissions/errors, and retain a protected manifest with the backup. A partial
   or locked copy is not a completed whole-state backup. Keep failure evidence
   linked to later repairs; do not overwrite it with a successful rerun.

Do not delete the only failed-operation input or evidence merely because a
directory is called `temp` or `cache`. WPM 1.x has no conservative general
cleanup classifier that certifies every such artifact as removable.

## Restore and validate before mutation

Restore the whole root while WPM, scripts, and handoffs are stopped. Prefer a
fresh empty destination, retaining the pre-restore state separately. Do not
merge an older snapshot over current live trust/configuration/package records.
Restore the intended account permissions and select the restored absolute root
through `WPM_DATA_DIR`; restore the correct runtime separately. Resolve external
paths and deployment state deliberately if the machine or locations changed.

Compare restored bytes/hierarchy with the protected backup manifest, then use
read-only inspection before an install, upgrade, or removal:

```text
wpm --diagnose
wpm list
wpm recovery list --json
wpm repo diagnose --links
wpm show <package>
```

Inspect partial/unavailable results and failure records instead of treating an
empty subset as proof of health. Read-only queries do not authenticate deployed
software or all retained archives. Reconcile trust and revocation state with
current policy, validate archive integrity/signatures against intended trusted
keys, and confirm externally deployed versions with package-specific guidance.
`repo verify <directory>` validates an authored local repository without scripts
or managed writes. Existing `verify <archive>` performs full archive validation
using managed staging; it is not the metadata-only, mutation-free query above.
Do not add a key merely to make a restored archive pass verification.

Pending self-upgrade material and audit references are historical inputs, not
instructions to launch an executable or replay a script. Use current package
maintainer guidance and revalidate inputs/current state before a new operation.
WPM cannot universally roll back arbitrary package-script effects. Copying only
`packages`, only trust keys, or only configuration is not a supported whole-state
restore procedure and may combine incompatible history, policies, or deployment.

## Evidence and delivery limits

The recovery integration fixture creates a signed install and failed upgrade,
copies the quiescent complete explicit data root into a fresh disposable root,
and compares its contents/hierarchy/timestamps. It reads retained identity and
failure evidence, verifies the local repository using the copied trust state,
and inspects repository health without writes or script replay. Both roots and
the external script marker remain unchanged during validation.

This is partial REQ-0019.006/.007 and I-06 delivery. The fixture does not certify
Windows ACL restoration, deployed-software recovery, live/pending handoffs,
administrator backup tooling, physical media/SMB, or interrupted-restore races.
Native release gates run the fixture on x86, x64, and ARM64; broader lifecycle
and real-environment obligations remain open. Existing deletion defaults and
persisted formats are unchanged; breaking contracts remain reserved for 2.x.
