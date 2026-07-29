"""Verify the controlled WPM release PDF and checksum."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

from pypdf import PdfReader


EXPECTED_TITLE = "Waughtal Package Manager Engineering Documentation"
EXPECTED_AUTHOR = "Waughtal"
EXPECTED_SUBJECT = "Controlled engineering and release documentation for WPM"
EXPECTED_REPOSITORY = "https://github.com/Thewafflication/wpm"


def fail(message: str) -> None:
    """Report one actionable validation error and terminate."""
    raise ValueError(message)


def verify_checksum(pdf_path: Path, checksum_path: Path) -> None:
    """Verify one exact SHA-256 checksum entry for the PDF."""
    if not checksum_path.is_file():
        fail(f"Checksum file was not found: {checksum_path}")
    lines = [line.strip() for line in checksum_path.read_text().splitlines()]
    lines = [line for line in lines if line]
    if len(lines) != 1:
        fail("SHA256SUMS must contain exactly one non-empty entry")
    expected = f"{hashlib.sha256(pdf_path.read_bytes()).hexdigest()}  {pdf_path.name}"
    if lines[0] != expected:
        fail("SHA256SUMS does not identify the exact release PDF bytes")


def count_links(reader: PdfReader) -> tuple[int, bool]:
    """Count link annotations and locate the displayed repository link."""
    count = 0
    repository_found = False
    for page in reader.pages:
        for annotation_ref in page.get("/Annots", []):
            annotation = annotation_ref.get_object()
            if annotation.get("/Subtype") != "/Link":
                continue
            count += 1
            action = annotation.get("/A")
            if action and action.get("/URI") == EXPECTED_REPOSITORY:
                repository_found = True
    return count, repository_found


def verify_pdf(pdf_path: Path, version: str) -> None:
    """Verify PDF structure, identity, navigation, links, and text."""
    if not pdf_path.is_file() or pdf_path.stat().st_size == 0:
        fail(f"Release PDF is missing or empty: {pdf_path}")
    reader = PdfReader(str(pdf_path))
    if len(reader.pages) < 10:
        fail(f"Expected at least 10 pages, found {len(reader.pages)}")

    metadata = reader.metadata or {}
    expected_metadata = {
        "/Title": EXPECTED_TITLE,
        "/Author": EXPECTED_AUTHOR,
        "/Subject": EXPECTED_SUBJECT,
    }
    for key, expected in expected_metadata.items():
        if str(metadata.get(key, "")).strip() != expected:
            fail(f"PDF metadata {key} does not equal {expected!r}")
    if version not in str(metadata):
        fail(f"PDF metadata does not identify version {version!r}")
    if str(reader.trailer["/Root"].get("/Lang", "")) not in {
        "en-US",
        "en-US ",
    }:
        fail("PDF catalog language is not en-US")

    try:
        outline = reader.outline
    except Exception as error:  # pragma: no cover - diagnostic boundary
        fail(f"PDF outline could not be read: {error}")
    if not outline:
        fail("PDF has no bookmarks/outline")

    extracted = []
    for number, page in enumerate(reader.pages, start=1):
        width = float(page.mediabox.width)
        height = float(page.mediabox.height)
        if width <= 0 or height <= 0:
            fail(f"Page {number} has invalid geometry")
        text = page.extract_text() or ""
        if not text.strip():
            fail(f"Page {number} has no extractable text")
        extracted.append(text)
    all_text = "\n".join(extracted)
    for marker in ("Contents", "Build Information", version):
        if marker not in all_text:
            fail(f"Extracted PDF text does not contain {marker!r}")

    link_count, repository_found = count_links(reader)
    if link_count == 0:
        fail("PDF contains no link annotations")
    if not repository_found:
        fail("PDF does not contain the expected repository hyperlink")

    print(
        f"Verified release PDF: pages={len(reader.pages)}, "
        f"links={link_count}, version={version}."
    )


def main() -> int:
    """Parse command-line arguments and run controlled verification."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--pdf", type=Path, required=True)
    parser.add_argument("--checksum", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    try:
        verify_pdf(args.pdf, args.version)
        verify_checksum(args.pdf, args.checksum)
    except Exception as error:
        print(f"release-documentation verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
