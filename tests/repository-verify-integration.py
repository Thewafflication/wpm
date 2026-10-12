"""Verify author catalogs, archive identities, trust, and zero mutations."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

from repository_test_helpers import junction

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())


def snapshot(root):
    return {
        str(path.relative_to(root)): (
            "directory"
            if path.is_dir()
            else hashlib.sha256(path.read_bytes()).hexdigest(),
            path.stat().st_mtime_ns,
        )
        for path in sorted(root.rglob("*"))
    }


with tempfile.TemporaryDirectory(prefix="wpm-repo-verify-") as directory:
    root = Path(directory)
    repository = root / "repository"
    repository.mkdir()
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    marker = root / "script-ran"
    (meta / "package.txt").write_text(
        "name=verify-check\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    (meta / "install.cmd").write_text(
        f'@echo off\necho ran>"{marker}"\nexit /b 0\n'
    )
    (source / "payload").write_text("verified payload")
    data = root / "data"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

    def run(*arguments, succeeds=True, readonly=False):
        before = snapshot(root)
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="",
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        if readonly:
            assert snapshot(root) == before
            assert not marker.exists()
        return result.stdout

    def verify(succeeds=True, *options):
        return run(
            "repo",
            "verify",
            repository,
            *options,
            succeeds=succeeds,
            readonly=True,
        )

    index = repository / "index.json"
    index.write_text('{"version":1,"packages":[]}')
    assert "verified (0 packages)" in verify()
    assert not data.exists()
    env["WPM_LOG_FILE"] = str(root / "external.log")
    env["WPM_LOG_DIR"] = str(root / "external-logs")
    verify(True, "--verbose")
    run("repo", "--verbose", "verify", repository, readonly=True)
    run("repo", "verify", "--verbose", repository, readonly=True)
    assert not data.exists()
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR"):
        env.pop(variable)
    private, public = root / "private", root / "public"
    run("keygen", private, public)
    run("build", source, repository, "--sign", private)
    archive = repository / "verify-check-any-1.0.0.zip"
    entry = {
        "name": "verify-check",
        "version": "1.0.0",
        "arch": "any",
        "url": archive.name,
    }

    def catalog(entries):
        index.write_text(json.dumps({"version": 1, "packages": entries}))

    catalog([entry])
    verify(False)  # Unknown keys never trigger an interactive trust addition.
    verify(False, "--allow-unsigned")  # Cannot bypass a signed unknown key.
    run("trust", "add", public)
    assert "verified (1 packages)" in verify()
    run("--verbose", "repo", "verify", repository, readonly=True)
    for field, value in (
        ("name", "other"),
        ("version", "1.0.1"),
        ("arch", "x86"),
        ("version", "invalid"),
        ("arch", "unknown"),
        ("url", "missing.zip"),
        ("url", "../private"),
        ("url", str(archive)),
        ("url", "https://example.invalid/archive.zip"),
    ):
        catalog([dict(entry, **{field: value})])
        verify(False)
    for field in entry:
        incomplete = dict(entry)
        del incomplete[field]
        catalog([incomplete])
        verify(False)
        catalog([dict(entry, **{field: 1})])
        verify(False)
    for entries in (
        [entry, entry],
        [None],
        [dict(entry, version=f"1.0.{number}") for number in range(97)],
    ):
        catalog(entries)
        verify(False)
    for text in (
        '{"version":1,"version":2,"packages":[]}',
        '{"version":1,"packages":[],"packages":[]}',
        '{"version":1.00000000000000000001,"packages":[]}',
        '{"version":1,"packages":[{"name":"first","name":"second",'
        '"version":"1.0.0","arch":"any","url":"archive.zip"}]}',
        '{"version":1,"packages":[}',
    ):
        index.write_text(text)
        verify(False)
    catalog([dict(entry, extra={"nested": [True, None, "value"]})])
    verify()
    catalog([entry])
    original = archive.read_bytes()
    with zipfile.ZipFile(archive, "a") as package:
        package.writestr("unindexed", "must reject")
    verify(False)
    archive.write_bytes(original)
    verify()
    outside = root / "outside"
    outside.mkdir()
    (outside / archive.name).write_bytes(original)
    link = repository / "linked"
    if junction(link, outside):
        try:
            catalog([dict(entry, url=f"linked/{archive.name}")])
            verify(False)
        finally:
            link.rmdir()
            catalog([entry])
    else:
        print("SKIP: fixture filesystem cannot create a directory junction")
    with zipfile.ZipFile(archive) as package:
        key_id = json.loads(package.read(".wpm/signature.json"))["key_id"]
    run("trust", "revoke", key_id)
    verify(False)
    # Unsigned archives require the explicit opt-in and the existing data rule.
    unsigned = root / "unsigned"
    unsigned.mkdir()
    (meta / "signature.json").unlink()
    run("build", source, unsigned)
    archive.write_bytes((unsigned / archive.name).read_bytes())
    verify(False)
    verify(True, "--allow-unsigned")
    for options in (
        ("--offline",),
        ("--links",),
        ("--priority", "1"),
        ("--allow-insecure-http",),
        ("--dry-run",),
        ("extra",),
    ):
        run(
            "repo",
            "verify",
            repository,
            *options,
            succeeds=False,
            readonly=True,
        )
    run("repo", "list", "--allow-unsigned", succeeds=False, readonly=True)
    run(
        "repo",
        "verify",
        "https://example.invalid",
        succeeds=False,
        readonly=True,
    )

print("PASS: author catalog/archive/trust verification without mutations")
