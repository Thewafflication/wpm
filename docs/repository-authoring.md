# Fresh repository initialization (1.2.24)

`wpm repo init <directory>` creates a fresh filesystem repository:

```text
repository/
  index.json       schema 1, empty packages array
  packages/        package publication directory
  keys/            public-key distribution directory
```

The target may be an existing ordinary directory or a new directory beneath an
existing parent. Relative paths resolve against the command's working directory.
Web URL targets are rejected. Existing unrelated files remain untouched.
Initialization never replaces an existing `index.json` or publication candidate;
layout paths must be directories without reparse points. The candidate is
created exclusively, flushed, and renamed without replacement, so a concurrent
index creation cannot be overwritten.

Initialization does not configure the repository or add trusted keys. To consume
it, use `wpm repo add <directory>` and `wpm update --offline`. No package scripts
run. Ordinary commands retain operational logging; `--dry-run` disables all
managed writes and previews the canonical target, directories, and empty index.
It works before the command or after the action/path. Invalid argument shapes
and inappropriate flags fail before initialization. A preview does not prove
future write permissions or connectivity.

Filesystem paths, including UNC paths, use the caller's Windows access. Offline
and read-only guarantees prohibit WPM web retrieval and managed mutations;
OS-managed filesystem access may involve an SMB share. No share credentials are
stored, printed, or supplied by this command.

`tests/repository-init-integration.py` verifies fresh and relative initialization,
complete dry-run snapshots including absent data/log roots, existing-index and
candidate protection, conflicting directory/file paths, missing parents,
invalid flags, and consumption through the existing version-1 resolver.

## Local catalog verification (1.2.26)

`wpm repo verify <directory> [--allow-unsigned]` checks one filesystem catalog
and all of its package archives without extraction, scripts, web retrieval,
trust changes, cache updates, or logs. It returns 0 after complete verification
and 1 on any failure. Empty initialized repositories are valid. Filesystem/UNC
reads use the caller's Windows access.

The catalog must have one schema-1 version and packages array. Every entry
needs a nonempty name, semantic version, supported architecture, and contained
relative archive path. Duplicate required fields or package identities, more
than 96 entries, missing files, and reparse-point layouts fail verification.
Unknown JSON extension values remain supported. Archive metadata must match
the catalog identity; archive paths, CRCs, indexed content hashes, completeness,
and existing signature trust are checked using the read-only validator.
Catalog and archive handles deny writes/deletion while each is validated.

Unknown or revoked signing keys fail without a prompt. Independently verify a
public key before adding it with `wpm trust add`. The optional unsigned flag
permits genuinely unsigned archives under the existing authorized-data-root
rule; it cannot bypass an invalid, unknown, or revoked signed archive.

This verifies the selected local catalog and its archives, not a signature on
the repository catalog itself. Linked roots and remote archive URLs are not
followed; use `repo diagnose --links` to inspect configured linked availability.
Verification does not prove future connectivity, write permissions, or script
behavior. Directory checks do not create a filesystem isolation boundary.

`tests/repository-verify-integration.py` covers empty and signed repositories,
absent data/log roots, trust rejection and revocation, identity mismatches,
malformed/duplicate/missing entries, archive tampering, unsigned opt-in, invalid
flags, and complete filesystem snapshots with no script execution.

## Immutable package publication (1.2.28)

`wpm repo add-package <directory> <archive> [--allow-unsigned] [--dry-run]`
appends a validated package to an initialized filesystem repository. Existing
catalog entries and the new archive must pass the same identity, integrity,
and existing-trust checks as `repo verify`. No package scripts run. Duplicate
name/architecture/version identities are refused; publication does not replace
an existing version. The 96-entry catalog and 4 MiB index bounds still apply.

Published archives use `packages/<BLAKE2b-256-of-archive>.zip` paths. These ASCII
content-addressed names avoid transport ambiguity for names containing spaces
or URL punctuation. Existing files at that path are reused only when their
complete bytes match; different bytes and existing candidates are protected.
Copies are exclusive, checked against source size and digest, flushed, and
renamed without replacement. Source handles deny writes/deletion during use.

The new schema-1 entry is inserted into the existing packages array. Bytes
outside that insertion, including extension fields, formatting, and linked
roots, are preserved. Links are preserved without following them. Catalog
publication uses an exclusive `.index.json.new`, flush, and atomic replacement;
failed publication retains the prior catalog. A completed but unreferenced
archive is retained for safe retry if catalog publication fails.

An exclusive `.wpm-publish.lock` serializes cooperating publishers and is removed
by Windows when its handle closes, including process termination. Keep other
catalog editors and directory changes quiescent during publication. The lock
does not coordinate external writers or create a filesystem isolation boundary.
Existing lock/candidate files are never automatically removed. Inspect retained
candidates and ensure no publisher is active before handling interrupted work.

`--dry-run` checks and previews the target archive/catalog without locks, copies,
catalog updates, data/trust/cache/log writes, or scripts. It supports placement
before the command or among action operands/options. A preview does not prove
future write permissions or exclude a later concurrent change. Publication
does not configure a repository or distribute/add signing keys automatically.
Consume it with `repo add`, `update --offline`, and the existing named install
workflow; `repo verify` provides a read-only post-publication check.

`tests/repository-publish-integration.py` covers signed and unsigned opt-in,
dry-run snapshots, unknown trust, duplicate identities, owned candidate/lock
collisions, archive-byte conflicts, exact extension/link preservation, second
versions, real catalog locking with safe retry/reuse, junction rejection, and
installation through the unchanged schema-1 resolver.

This remains a partial I-09 delivery. Deterministic full index regeneration,
repository signatures, and remaining media/network matrix evidence stay open.
Breaking changes remain reserved for 2.x.

## Explicit index signing (1.2.39)

```text
wpm repo sign .\repository --sign .\keys\index.private --index-key .\keys\index.public --dry-run
wpm repo sign .\repository --sign .\keys\index.private --index-key .\keys\index.public
wpm repo verify .\repository --index-key .\keys\index.public
```

`repo sign` writes a detached `index.sig` over the exact existing `index.json`
bytes, including whitespace/BOM/extension/link bytes. It does not regenerate or
change the catalog or archives. The explicit private/public pair must match;
private-key input is kept outside the repository and no private bytes are copied
there or printed. Secret decode/signing buffers are cleared. Package archives
are first validated under existing package-signing trust, independently of the
chosen index signer. Unsigned packages still need existing explicit authorization
through `--allow-unsigned`; signing the catalog does not authenticate them.

The public JSON sidecar declares numeric `version` 1, `purpose` equal to
`repository-index`, `algorithm` equal to `ed25519`, lowercase 64-character
`key_id`, and base64 `public_key` and `signature`. The key identifier uses the
existing public-key identifier rule. Ed25519 covers the exact catalog bytes,
consistent with ADR-0012. The purpose/schema declaration identifies this
sidecar for explicit index verification; it does not authorize package keys.

`repo verify --index-key <public-file>` requires this signature and the exact
public key selected by the caller for this root. Obtain/check that key through
an independent channel before selecting it. A key found in repository content
or the package trust store does not become index authority automatically.
The index and package roles can intentionally use identical material, but each
role needs its own explicit selection/authorization. Missing, stale, malformed,
unsupported, duplicate-field, wrong-key, or altered signatures fail closed
without prompts, trust additions, cache preparation, or managed writes. Signature
verification precedes opening the referenced package archives; package integrity,
identity, and existing trust remain separately required.

Index-key selection is per invocation in this compatible delivery. Persistent
repository-scoped pin configuration, signed-cache admission, and signed web/media
consumption remain open. Existing consumers, `repo verify` without `--index-key`,
and install/update flows retain legacy package-authenticated behavior and do not
check `index.sig`. Adding a sidecar does not silently strengthen that policy.
The catalog/package/signature formats already consumed by 1.x are unchanged.

An existing signature is not replaced unless `--replace-signature` explicitly
authorizes replacement of that root's `index.sig`. A fresh `.index.sig.new`
candidate is created without overwriting existing candidates, flushed, and
atomically renamed. The existing catalog is held against writes/deletion while
its signature is computed/published. Real writes share the publisher's exclusive
`.wpm-publish.lock`; the lock is automatically removed when its handle closes.
Existing locks/candidates and leaf reparse/directory conflicts fail without
changing user-owned files. A failed replacement leaves the old signature intact
and cleans only the writer's own candidate. Keep directory/volume mappings and
external editors quiescent; the cooperative lock is not a filesystem sandbox.

`--dry-run` validates catalog/archive state, public signer selection, private-path
attributes, destination policy, and conflicts. It does not open/read private-key
bytes, check the private/public pair, sign, create a lock/candidate, initialize
managed roots/logs, or mutate files. Pair validity is checked only during actual
signing. A private file that is unreadable or has invalid contents may therefore
still produce a public-only dry-run plan if its attributes are accessible.

Catalog changes invalidate the old byte signature, including `add-package` and
whitespace-only edits. The publisher leaves existing sidecars intact rather than
silently deleting evidence or guessing a signing key. Re-sign deliberately after
publication, then use explicit-key verification before distributing a complete
snapshot. Existing signature replacement is not a universal transaction across
catalog, archives, and external copies; default consumption remains unchanged.

Fixtures use separate generated package and index keys, proving package trust
does not authorize the index role and index verification does not bypass revoked
package trust. Cases cover unknown/missing/wrong authority, exact-byte changes,
key mismatch, malformed descriptors, duplicate fields/options, protected locks
and candidates, locked replacement, private input inside the root, batch CLI
shapes, and public-only dry-run with an absent data/log root. Snapshots verify
no read-only mutation or script replay. This is partial REQ-0017/I-09 delivery;
the full signer-policy/consumer/media/copy and authoring completion gates remain
open, with breaking changes reserved for 2.x.

## Deterministic package-array rebuilding (1.2.40)

```text
wpm repo index .\repository --dry-run
wpm repo index .\repository
wpm repo sign .\repository --sign .\keys\index.private --index-key .\keys\index.public --replace-signature
wpm repo verify .\repository --index-key .\keys\index.public
```

`repo index` inventories direct `packages/*.zip` files in an initialized,
ordinary filesystem/UNC repository. It validates every selected archive's
paths, integrity, existing trust, metadata, semantic version, and supported
architecture, rejecting duplicate name/architecture/version identities. It
holds all selected input files against writes/deletion through publication.
Unsigned input requires explicit `--allow-unsigned` and existing authorization;
there are no trust prompts or script/extraction operations.

Leaf filenames must contain only ASCII letters/digits and `._+-`, giving relative
`packages/<leaf>` locators that do not require a new 1.x URL/path encoding rule.
Use `repo add-package` to publish an archive under an opaque safe filename when
its original leaf is not portable. Directories/reparse entries, invalid metadata,
conflicting identities, unreadable files, or more than 96 selected archives fail
without replacing the old catalog. Nested directories, root-level archives,
non-ZIP files, and broader unindexed-file findings are outside this rebuild set.

Rows sort by case-insensitive name/architecture, descending semantic version,
full version string, and relative locator. The package-array content becomes a
compact deterministic representation; every byte outside that array is retained,
including linked repositories, extensions, whitespace, and BOM. The existing
catalog must have supported grammar/schema and valid entry shapes, but stale
entry/archive mappings can be replaced from the package directory. Rebuilding
an already identical array preserves catalog bytes and modification time.

Real updates share the cooperative author lock and use a create-new, flushed
`.index.json.new` candidate and atomic replacement. Existing candidates/locks,
unsupported roots, locked replacement, and validation failures preserve prior
catalog bytes; only the writer's own failed candidate is cleaned up. Directory
mappings and external editors must remain quiescent. Dry-run reads/validates and
previews the same bounded result without locks, writes, managed initialization,
logs, or extraction. Normal execution remains a mutating command even when its
catalog is already current.

An existing `index.sig` is retained as evidence and is not automatically updated.
Catalog byte changes make it stale; explicitly re-sign and verify before copying
a complete distribution tree. This is not an atomic catalog/signature transaction
or an implicit private-key selection. Existing install/update/resolver defaults,
configuration, and version-1 formats remain unchanged.

The fixture covers signed archives with semantic ordering, preserved linked and
extension bytes, dry-run with absent data/log roots, unchanged repeated output,
duplicates/bad/unportable leaves, candidate/lock conflicts, locked input/target,
96/97 archive bounds, explicit unsigned policy, an NTFS package-directory junction,
invalid CLI shapes, and snapshots without scripts. I-09 remains partial for
full confirmation, complete finding collection, integrated sign/index transactions,
consumer pin/cache policy, copy/media, and physical environment evidence.

From 1.2.41, selected private/public key files must contain a complete supported
version-1 header token; a `v10` or longer suffix is not interpreted as `v1`.
Generated version-1 key files and LF/CRLF line endings remain supported. This
schema check does not make a repository-distributed key an independent authority.

The 1.2.42 native journey copies a complete signed repository into a fresh
distribution directory, verifies it with an independently selected index key,
then configures that copy and installs its signed package offline using relative
locators. The consumer trusts the package key separately. Repository bytes and
modification times remain unchanged during verification and consumption.
This fixture demonstrates filesystem-copy portability; physical removable media,
network shares, and access-control/read-only media evidence remain open.

## Repeatable copied-distribution journey (1.2.43)

These PowerShell examples assume an existing signed `demo.zip` whose package
name is `demo`, a separately verified package public key, and a matching index
private/public pair. Key files stay outside both distribution trees. Select
keys through your existing independent authorization process; copying a key
from the distribution alone does not establish authority.

In the author's shell, start with fresh `repository` and `distribution-copy`
destinations beneath the current directory. Check every command succeeds before
continuing; stop on a nonzero WPM exit code or a copy error.

```powershell
$packageKey = '.\keys\package.public'
$indexPrivate = '.\keys\index.private'
$indexPublic = '.\keys\index.public'
wpm trust add $packageKey
wpm repo init .\repository
wpm repo add-package .\repository .\demo.zip
wpm repo index .\repository --dry-run
wpm repo index .\repository
wpm repo sign .\repository --sign $indexPrivate --index-key $indexPublic
wpm repo verify .\repository --index-key $indexPublic
Copy-Item -LiteralPath .\repository -Destination .\distribution-copy -Recurse
wpm repo verify .\distribution-copy --index-key $indexPublic
```

Copy the entire tree, including `index.json`, `index.sig`, and `packages`;
retain relative package paths. The copy destination can be an optical-mastering
directory before an external utility writes the medium. If catalog bytes change
later, explicitly re-sign using `--replace-signature` and verify again.

In a fresh consumer shell, use an explicit, separate writable data directory.
Provision the public keys independently; the consumer needs no private key.
The copied repository remains the read source, separate from consumer state.

```powershell
$env:WPM_DATA_DIR = "$PWD\consumer-data"
$packageKey = '.\keys\package.public'
$indexPublic = '.\keys\index.public'
wpm trust add $packageKey
wpm repo verify .\distribution-copy --index-key $indexPublic
wpm repo add .\distribution-copy --priority 1000
wpm update --offline
wpm install demo --offline
wpm list demo
```

The explicit verification checks catalog authority and package trust before
configuration. Legacy update/install still use their existing package validation
and resolver rules; they do not persist the index-key selection or enforce
`index.sig`. Installation executes the package's existing authorized scripts.
Offline mode prohibits managed web retrieval but does not certify package script
network behavior. This journey does not claim physical-media or ACL evidence.
