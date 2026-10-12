"""Verify retained metadata listing, selectors, bounds, and zero mutations."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

from repository_test_helpers import junction, read_lock

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


with tempfile.TemporaryDirectory(prefix="wpm-installed-list-") as directory:
    root = Path(directory)
    data = root / "data"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    env["WPM_LOG_FILE"] = str(root / "external.log")
    env["WPM_LOG_DIR"] = str(root / "external-logs")

    def run(*arguments, succeeds=True, readonly=False):
        before = snapshot(root) if readonly else None
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="",
            text=True,
            encoding="utf-8" if "--json" in arguments else None,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        if "--json" in arguments:
            assert result.stderr == "", result.stderr
        if readonly:
            assert snapshot(root) == before
        return result.stdout

    def machine(*arguments, succeeds=True):
        text = run(*arguments, "--json", succeeds=succeeds, readonly=True)
        assert "\x1b" not in text and text.endswith("\n"), text
        value = json.loads(text)
        assert value["schema"] == "wpm.package-query.v1", value
        assert value["mode"] == "retained" and value["catalog"] is None
        assert value["metadata_only"] is True, value
        assert value["archive_integrity_verified"] is False, value
        assert value["signature_trust_verified"] is False, value
        assert value["deployed_state_known"] is False, value
        assert value["exit_code"] == (0 if succeeds else 1), value
        return value

    value = machine("list")
    assert value["retained"]["records"] == [] and value["complete"], value
    run("list", "--json", "--verbose", succeeds=False, readonly=True)
    for arguments in (
        ("list",),
        ("list", "missing", "--offline"),
        ("--verbose", "list"),
    ):
        text = run(*arguments, readonly=True)
        assert "0 matching retained records" in text, text
        assert not data.exists()
        assert not (root / "external.log").exists()
    for arguments in (
        ("list", "one", "two"),
        ("list", "--arch", "unknown"),
        ("list", "--version", "invalid"),
        ("list", "--arch"),
        ("list", "--yes"),
        ("list", "--dry-run"),
    ):
        run(*arguments, succeeds=False, readonly=True)
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR"):
        env.pop(variable)
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    (meta / "install.cmd").write_text("@echo off\nexit /b 0\n")
    (source / "payload").write_text("listed package payload")
    archives = root / "archives"
    archives.mkdir()
    marker = root / "query-script-ran"
    private, public = root / "private", root / "public"
    run("keygen", private, public)
    run("trust", "add", public)

    def build(name, version, arch):
        (meta / "package.txt").write_text(
            f"name={name}\nversion={version}\narch={arch}\ndebug=false\n"
        )
        run("build", source, archives, "--sign", private)
        return archives / f"{name}-{arch}-{version}.zip"

    original = build("listed-check", "1.0.0", "any")
    run("install", original)
    store = data / "packages"
    (meta / "install.cmd").write_text(
        f'@echo off\necho ran>"{marker}"\nexit /b 0\n'
    )
    for version in ("2.0.0", "10.0.0", "10.0.0-beta.1", "10.0.0+build.1"):
        archive = build("listed-check", version, "any")
        (store / archive.name).write_bytes(archive.read_bytes())
    archive = build("listed-check", "2.0.0", "x86")
    (store / archive.name).write_bytes(archive.read_bytes())
    archive = build("another-check", "1.0.0", "any")
    (store / archive.name).write_bytes(archive.read_bytes())
    text = run("list", readonly=True)
    assert "7 matching retained records" in text, text
    assert "trust and deployed software are not verified" in text, text
    expected = ["10.0.0", "10.0.0+build.1", "10.0.0-beta.1", "2.0.0", "1.0.0"]
    rows = [
        line.strip()
        for line in text.splitlines()
        if line.startswith("  listed-check any ")
    ]
    assert rows == [f"listed-check any {version}" for version in expected], rows
    for arguments, count in (
        (("list", "LISTED-CHECK"), 6),
        (("list", "--arch", "x86", "listed-check"), 1),
        (("list", "listed-check", "--version", "2.0.0"), 2),
        (("list", "--arch", "any", "--version", "2.0.0"), 1),
        (("list", "--version", "99.0.0"), 0),
        (("list", "missing"), 0),
    ):
        text = run(*arguments, readonly=True)
        assert f"{count} matching retained records" in text, text
        value = machine(*arguments)
        assert len(value["retained"]["records"]) == count, value
        assert value["retained"]["summary"]["matches"] == count, value
    run("list", "--verbose", readonly=True)
    assert not marker.exists()
    bad = store / "bad.zip"
    bad.write_text("invalid archive")
    text = run("list", succeeds=False, readonly=True)
    assert "record issues: 1" in text, text
    value = machine("list", succeeds=False)
    assert value["status"] == "partial", value
    assert value["retained"]["summary"]["issues"] == 1, value
    bad.unlink()
    before = snapshot(root)
    with read_lock(store / original.name, share=0):
        run("list", succeeds=False)
    assert snapshot(root) == before
    secret = "DO_NOT_LEAK_QUERY_CREDENTIAL"
    with zipfile.ZipFile(bad, "w") as package:
        package.writestr(
            ".wpm/package.txt",
            f"name=https://user:{secret}@example.invalid\n"
            "version=1.0.0\narch=any\ndebug=false\n",
        )
    text = run("list", succeeds=False, readonly=True)
    assert secret not in text
    assert secret not in json.dumps(machine("list", succeeds=False))
    bad.unlink()
    invalid_metadata = store / "invalid-text.zip"
    with zipfile.ZipFile(invalid_metadata, "w") as package:
        package.writestr(
            ".wpm/package.txt", b"name=bad-\x80\nversion=1.0.0\narch=any\n"
        )
    value = machine("--color", "always", "list", succeeds=False)
    assert value["retained"]["summary"]["issues"] == 1, value
    assert value["retained"]["summary"]["serialization_issues"] == 0, value
    invalid_metadata.unlink()
    copies = []
    for number in range(250):
        copy = store / f"fixture-{number:03d}.zip"
        copy.write_bytes(original.read_bytes())
        copies.append(copy)
    text = run("list", succeeds=False, readonly=True)
    assert "limited: yes" in text, text
    value = machine("list", succeeds=False)
    assert value["retained"]["summary"]["limited"] is True, value
    for copy in copies:
        copy.unlink()
    saved = data / "saved-packages"
    store.rename(saved)
    if junction(store, saved):
        try:
            run("list", succeeds=False, readonly=True)
            value = machine("list", succeeds=False)
            assert value["retained"]["readable"] is False, value
        finally:
            store.rmdir()
    else:
        print("SKIP: fixture filesystem cannot create a directory junction")
    saved.rename(store)
    run("list", readonly=True)
    assert not marker.exists()

print("PASS: retained installed metadata queries without writes or scripts")
