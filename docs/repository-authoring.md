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

This is a partial I-09 delivery. Package publication, deterministic index
regeneration, repository signatures/verification, and remaining media/network
matrix evidence stay open. Breaking changes remain reserved for 2.x.
