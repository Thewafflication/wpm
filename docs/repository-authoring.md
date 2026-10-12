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

This is a partial I-09 delivery. Package publication, deterministic index
regeneration, repository signatures, and remaining media/network matrix
evidence stay open. Breaking changes remain reserved for 2.x.
