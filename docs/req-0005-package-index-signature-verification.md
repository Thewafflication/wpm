# REQ-0005 - Package Index Signature Verification

## Description

The `wpm build` command shall populate `.wpm/index.csv` with file size and
BLAKE2b file signatures unless `--no-index` is specified.

When building a package, the application shall:

- write an index header of `filename,size,hash,algorithm`,
- recursively index package files using archive-relative paths,
- record each indexed file's size in bytes,
- record each indexed file's BLAKE2b hash with `blake2b` as the algorithm,
- omit files matched by `.wpm/wpmignore.txt`,
- always index package support files `.wpm/package.txt`, `.wpm/install.cmd`,
  `.wpm/remove.cmd`, and `.wpm/wpmignore.txt` using relative archive paths,
- avoid indexing `.wpm/index.csv` itself, and
- include the populated index in the generated archive.

The `wpm install` command shall verify indexed files after staging and before
executing `.wpm/install.cmd`.

When installing a package that contains `.wpm/index.csv`, the application
shall:

- verify each indexed file exists after extraction,
- verify each indexed file's byte size,
- recompute each indexed file's BLAKE2b signature, and
- fail installation if any indexed file does not match its recorded signature.

## Rationale

Package authors and installers need a package-local integrity record so WPM can
detect accidental corruption or tampering before treating extracted contents as
successfully installed.

## Verification

Verified by:

- TC-0005 - Package index signature verification
