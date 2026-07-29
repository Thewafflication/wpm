# REQ-0005: Package Index Signature Verification

**Content type:** Project requirements

**Status:** Accepted

**Source:** ADR-0001 package trust model

## Scope

Applies when WPM builds or validates indexed package contents.

## Requirement

**REQ-0005.001**
The `wpm build` command shall populate `.wpm/index.csv` with file size and
BLAKE2b file signatures unless `--no-index` is specified.

**REQ-0005.002**
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

**REQ-0005.003**
The `wpm install` command shall verify indexed files after staging and before
executing `.wpm/install.cmd`.

**REQ-0005.004**
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

**Method:** Automated test and inspection

**References:** TC-0005 and `tests/tc-0005-*.ps1`

Verified by:

- TC-0005 - Package index signature verification

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0005-package-index-signature-verification.ps1` currently implement and verify this requirement.
