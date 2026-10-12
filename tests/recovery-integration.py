"""Inspect a real failed upgrade without mutation or script replay."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

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


with tempfile.TemporaryDirectory(prefix="wpm-recovery-") as directory:
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
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        if readonly:
            assert snapshot(root) == before
        return result.stdout

    for arguments in (
        ("recovery",),
        ("recovery", "list"),
        ("--verbose", "recovery", "list"),
        ("recovery", "--verbose", "list", "--offline"),
    ):
        text = run(*arguments, readonly=True)
        assert "0 recorded upgrade failures" in text, text
        assert not data.exists()
        assert not (root / "external.log").exists()
    for arguments in (
        ("recovery", "retry"),
        ("recovery", "list", "extra"),
        ("recovery", "list", "--yes"),
        ("recovery", "list", "--dry-run"),
        ("recovery", "list", "--json"),
    ):
        run(*arguments, succeeds=False, readonly=True)
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR"):
        env.pop(variable)
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    marker = root / "script-marker"
    (meta / "package.txt").write_text(
        "name=recovery-check\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    (meta / "install.cmd").write_text("@echo off\nexit /b 0\n")
    (source / "payload").write_text("recovery fixture")
    repository = root / "repository"
    repository.mkdir()
    private, public = root / "private", root / "public"
    run("keygen", private, public)
    run("trust", "add", public)
    run("build", source, repository, "--sign", private)
    run("install", repository / "recovery-check-any-1.0.0.zip")
    (meta / "package.txt").write_text(
        "name=recovery-check\nversion=1.0.1\narch=any\ndebug=false\n"
    )
    (meta / "install.cmd").write_text(
        f'@echo off\necho failed>"{marker}"\nexit /b 7\n'
    )
    run("build", source, repository, "--sign", private)
    (repository / "index.json").write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [
                    {
                        "name": "recovery-check",
                        "version": "1.0.1",
                        "arch": "any",
                        "url": "recovery-check-any-1.0.1.zip",
                    }
                ],
            }
        )
    )
    run("repo", "add", repository, "--priority", "1000")
    run("update", "--offline")
    run(
        "upgrade",
        "recovery-check",
        "--arch",
        "any",
        "--offline",
        succeeds=False,
    )
    assert marker.exists()
    audit = next((data / "audit").glob("*.upgrade-failed.txt"))
    text = run("recovery", "list", readonly=True)
    assert "1 recorded upgrade failures" in text, text
    assert "install-script; exit code: 7" in text, text
    assert "1.0.0 -> 1.0.1" in text and audit.name in text, text
    assert "does not retry or clean up" in text, text
    original = audit.read_bytes()
    secret = b"DO_NOT_LEAK_AUDIT_CREDENTIAL"
    audit.write_bytes(
        original.replace(
            b"name=recovery-check",
            b"name=https://user:" + secret + b"@example.invalid",
        )
    )
    text = run("recovery", "list", readonly=True)
    assert secret.decode() not in text
    audit.write_bytes(original.rstrip(b"\n"))
    run("recovery", "list", readonly=True)
    for changed in (
        original + b"name=duplicate\n",
        original.replace(b"name=recovery-check", b"name=" + b"x" * 128),
        original.replace(b"exit-code=7", b"exit-code=4294967296"),
        original.replace(b"status=failed", b"status=upgraded"),
        original.replace(b"archive=", b"archive=../"),
        original + b"\0bad",
        b"x" * 16385,
    ):
        audit.write_bytes(changed)
        text = run("recovery", "list", succeeds=False, readonly=True)
        assert "invalid/unreadable records: 1" in text, text
    audit.write_bytes(original)
    before = snapshot(root)
    with read_lock(audit, share=0):
        run("recovery", "list", succeeds=False)
    assert snapshot(root) == before
    # More records than the fixed bound yield an explicit partial query.
    copies = []
    for number in range(128):
        copy = audit.parent / f"fixture-{number:03d}.upgrade-failed.txt"
        copy.write_bytes(original)
        copies.append(copy)
    text = run("recovery", "list", succeeds=False, readonly=True)
    assert "limited: yes" in text, text
    for copy in copies:
        copy.unlink()
    audit_dir = data / "audit"
    saved_dir = data / "saved-audit"
    audit_dir.rename(saved_dir)
    if junction(audit_dir, saved_dir):
        try:
            run("recovery", "list", succeeds=False, readonly=True)
        finally:
            audit_dir.rmdir()
    else:
        print("SKIP: fixture filesystem cannot create a directory junction")
    saved_dir.rename(audit_dir)
    run("recovery", "list", readonly=True)

print("PASS: real failed upgrade audit query without mutation or script replay")
