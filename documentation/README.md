# WPM Release Documentation Control

**Content type:** Controlled build procedure

**Status:** Accepted

**Owner:** WPM maintainers

The version-controlled `documentation-manifest.json` defines the identity,
source set, and chapter order of WPM's engineering release PDF. Markdown in the
repository and the pinned WSP submodule remains authoritative; generated TeX,
logs, rendered pages, checksums, and PDFs are outputs only.

## Build

From the repository root, with Pandoc 3+, MiKTeX, and PowerShell 7 available:

```powershell
pwsh -File wsp/tools/Build-Documentation.ps1 `
  -RepositoryRoot . `
  -ManifestPath documentation/documentation-manifest.json `
  -Pandoc documentation/pandoc.ps1 `
  -HeaderPath documentation/code-blocks.tex `
  -Version <release-version>
```

Intermediate output is written beneath `tmp/pdfs/`; the final document is
`output/pdf/wpm-engineering-documentation.pdf`. Generated output shall never be
written beneath `wsp/`.

The project header wraps long plain and highlighted code blocks so command
arguments and paths remain visible within the text area.
The project Pandoc adapter also gives Markdown tables bounded wrapping columns
and permits long identifiers/paths to wrap within cells. It uses Pandoc from
`PATH`, or the executable selected by `WPM_DOCUMENT_PANDOC`.
Controlled `.tex` test specifications are first parsed with Pandoc's LaTeX
reader and normalized under `tmp/pdfs/documentation/normalized`. Their field
values and procedures render as chapters instead of literal macro definitions;
the version-controlled specifications remain authoritative and unchanged.

## Verification

The controlled verifier checks PDF identity and metadata, page count,
extractable text, table of contents, outline/bookmarks, link annotations,
repository link, page geometry, and the exact SHA-256 checksum entry:

```powershell
python tests/verify-release-documentation.py `
  --pdf output/pdf/wpm-engineering-documentation.pdf `
  --checksum output/pdf/SHA256SUMS `
  --version <release-version>
```

CI renders every page to PNG and retains the rendering with the PDF. Before a
release is approved, a reviewer inspects the rendered pages for clipped or
overlapping text, broken tables, unreadable glyphs, inconsistent presentation,
and incorrect section transitions. The review is recorded in the release-
readiness record.

## Failure behavior

Missing or duplicate manifest entries, missing tools, Pandoc or LaTeX errors,
missing/empty output, verification failures, checksum mismatch, or rendering
failure return a nonzero result and block the documentation gate.

## Publication

Release automation publishes the exact verified PDF and `SHA256SUMS` beside the
WPM packages and generates GitHub build-provenance attestation for the PDF.
PAdES signing is not selected by the current WSP adoption record.
