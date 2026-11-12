"""Optional metadata survives signed publication, queries, and rebuilding."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
wpm = str(Path(parser.parse_args().wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-optional-metadata-") as directory:
    root = Path(directory)
    source, output, repository = root / "source", root / "out", root / "repo"
    source.mkdir()
    output.mkdir()
    data = root / "data"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for key in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(key, None)

    def snapshot():
        return {
            str(p.relative_to(root)): (p.read_bytes(), p.stat().st_mtime_ns)
            for p in root.rglob("*")
            if p.is_file()
        }

    def run(*arguments, succeeds=True, readonly=False):
        before = snapshot() if readonly else None
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            cwd=source,
            capture_output=True,
            input="",
            encoding="utf-8",
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        if readonly:
            assert snapshot() == before
        return result.stdout

    run("init", "metadata-check")
    metadata = source / ".wpm" / "package.txt"
    assert "tags=\n" in metadata.read_text()
    basic = "name=metadata-check\nversion=1.0.0\narch=any\ndebug=false\n"
    fields = {
        "description": 'A "quoted" tool with \\ paths and caf\u00e9',
        "tags": "cli, developer-tools, windows",
        "repo": "https://example.invalid/owner/project.git",
        "homepage": "https://example.invalid/project?x=1&y=2",
        "license": "MIT OR Apache-2.0",
        "maintainer": "Example Maintainer",
        "build_date": "2026-11-12",
    }
    authored = basic + "".join(f"{k}={v}\n" for k, v in fields.items())
    metadata.write_text(authored, encoding="utf-8")
    (source / "payload.bin").write_bytes(bytes(range(256)))
    run("keygen", root / "private", root / "public")
    run("trust", "add", root / "public")
    run("build", source, output, "--sign", root / "private")
    archive = output / "metadata-check-any-1.0.0.zip"
    run("verify", archive)
    with zipfile.ZipFile(archive) as package:
        assert package.read(".wpm/package.txt") == metadata.read_bytes()
    run("repo", "init", repository)
    run("repo", "add-package", repository, archive)
    index = repository / "index.json"
    catalog = json.loads(index.read_text(encoding="utf-8-sig"))
    entry = catalog["packages"][0]
    for key, value in fields.items():
        assert entry[key] == value, entry
    run("repo", "index", repository)
    rebuilt = index.read_bytes()
    assert json.loads(rebuilt)["packages"][0] == entry
    run("repo", "index", repository)
    assert index.read_bytes() == rebuilt
    run("repo", "verify", repository, readonly=True)
    run("repo", "add", repository)
    default = "https://github.com/Thewafflication/wpm/releases/latest/download"
    cache_id = 2166136261
    for byte in default.encode():
        cache_id = ((cache_id ^ byte) * 16777619) & 0xFFFFFFFF
    cache = data / "cache" / "repositories" / f"{cache_id:08x}.json"
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text('{"version":1,"packages":[]}')

    # Retained inspection reads the archive, without running its scripts.
    store = data / "packages"
    store.mkdir(parents=True, exist_ok=True)
    (store / archive.name).write_bytes(archive.read_bytes())
    for command in (
        ("show", "metadata-check"),
        ("list", "--available"),
        ("list",),
    ):
        text = run(*command, readonly=True)
        assert "description:" in text and "tags:" in text and "repo:" in text
        document = json.loads(run(*command, "--json", readonly=True))
        records = []
        for section in ("retained", "catalog"):
            records.extend((document.get(section) or {}).get("records", []))
        assert records, document
        for record in records:
            for key, value in fields.items():
                assert record[key] == value, record

    # Sorting multiple catalog rows must keep each row's optional values.
    other = dict(entry, name="aaa-check", description="other package")
    catalog["packages"].append(other)
    index.write_text(json.dumps(catalog), encoding="utf-8")
    document = json.loads(run("list", "--available", "--json", readonly=True))
    rows = document["catalog"]["records"]
    assert [(r["name"], r["description"]) for r in rows] == [
        ("aaa-check", "other package"),
        ("metadata-check", fields["description"]),
    ]
    catalog["packages"].pop()

    # Exact byte limits and worst-case quote escaping survive publication.
    maximum = {key: '\\"' * 511 + "x" for key in fields}
    metadata.write_text(
        basic + "".join(f"{k}={v}\n" for k, v in maximum.items()),
        encoding="utf-8",
    )
    run("build", source, output, "--sign", root / "private")
    bounds_repo = root / "bounds"
    run("repo", "init", bounds_repo)
    run("repo", "add-package", bounds_repo, archive)
    boundary_entry = json.loads((bounds_repo / "index.json").read_text())
    for key, value in maximum.items():
        assert boundary_entry["packages"][0][key] == value

    # Optional fields may all be absent or blank on older packages.
    for suffix in ("", "".join(f"{key}=\n" for key in fields)):
        metadata.write_text(basic + suffix, encoding="utf-8")
        run("build", source, output, "--sign", root / "private")
        run("verify", archive)

    # Reject oversized, terminal-control, and invalid UTF-8 values on build.
    for value in (b"x" * 1024, b"hello\x1b[31m", b"bad\xffutf8"):
        metadata.write_bytes(basic.encode() + b"description=" + value + b"\n")
        run("build", source, output, succeeds=False)

    # Catalog field types, bounds, controls, and duplicates are checked too.
    for value in ([], 42, None, "x" * 1024, "line\nline", "\x1b[31m"):
        entry["description"] = value
        index.write_text(json.dumps(catalog), encoding="utf-8")
        run("show", "metadata-check", succeeds=False, readonly=True)
    entry["description"] = fields["description"]
    text = json.dumps(catalog).replace(
        '"description":', '"description":"duplicate", "description":', 1
    )
    index.write_text(text, encoding="utf-8")
    run("show", "metadata-check", succeeds=False, readonly=True)

print("PASS: optional metadata, signed round trips, queries, and invalid input")
