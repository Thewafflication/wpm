"""Deterministic catalog rebuild, preserved links, failures, and dry-run."""

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


with tempfile.TemporaryDirectory(prefix="wpm-repository-index-") as directory:
    root = Path(directory)
    data = root / "data"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

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
        assert not (root / "script-ran").exists()
        return result.stdout

    repository = root / "repository"
    run("repo", "init", repository)
    index = repository / "index.json"
    prefix = b'{ "repositories": ["previous"], "version": 1, "packages": ['
    suffix = b'], "extension": {"kept":true} }\n'
    index.write_bytes(prefix + b" \n " + suffix)
    env["WPM_DATA_DIR"] = str(root / "absent-data")
    env["WPM_LOG_FILE"] = str(root / "absent.log")
    run("repo", "index", repository, "--dry-run", readonly=True)
    assert not (root / "absent-data").exists()
    env["WPM_DATA_DIR"] = str(data)
    env.pop("WPM_LOG_FILE")
    private, public = root / "private", root / "public"
    run("keygen", private, public)
    run("trust", "add", public)
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    (meta / "install.cmd").write_text(
        f'@echo off\necho ran>"{root / "script-ran"}"\n'
    )
    (source / "payload").write_text("indexed without scripts")
    archives = root / "archives"
    archives.mkdir()
    packages = repository / "packages"
    for version, arch, leaf in (
        ("2.0.0", "any", "z.zip"),
        ("10.0.0", "any", "a.zip"),
        ("2.0.0", "x86", "x86.zip"),
    ):
        (meta / "package.txt").write_text(
            f"name=indexed\nversion={version}\narch={arch}\ndebug=false\n"
        )
        run("build", source, archives, "--sign", private)
        (packages / leaf).write_bytes(
            (archives / f"indexed-{arch}-{version}.zip").read_bytes()
        )
    run("repo", "index", repository, "--dry-run", readonly=True)
    run("repo", "index", repository)
    rebuilt = index.read_bytes()
    assert rebuilt.startswith(prefix) and rebuilt.endswith(suffix)
    entries = json.loads(rebuilt)["packages"]
    assert [(entry["version"], entry["arch"]) for entry in entries] == [
        ("10.0.0", "any"),
        ("2.0.0", "any"),
        ("2.0.0", "x86"),
    ], entries
    assert all(entry["url"].startswith("packages/") for entry in entries)
    stamp = index.stat().st_mtime_ns
    run("repo", "index", repository)
    assert index.read_bytes() == rebuilt and index.stat().st_mtime_ns == stamp
    run("repo", "verify", repository, readonly=True)
    signature = repository / "index.sig"
    signature.write_text("retained signature evidence")
    run("repo", "index", repository)
    assert signature.read_text() == "retained signature evidence"
    for leaf, content in (
        ("duplicate.zip", (packages / "a.zip").read_bytes()),
        ("bad.zip", b"not an archive"),
        ("not portable.zip", (packages / "a.zip").read_bytes()),
    ):
        bad = packages / leaf
        bad.write_bytes(content)
        run("repo", "index", repository, succeeds=False)
        assert index.read_bytes() == rebuilt
        assert not (repository / ".index.json.new").exists()
        bad.unlink()
    candidate = repository / ".index.json.new"
    candidate.write_text("user-owned candidate")
    run("repo", "index", repository, succeeds=False)
    assert candidate.read_text() == "user-owned candidate"
    candidate.unlink()
    lock = repository / ".wpm-publish.lock"
    lock.write_text("user-owned lock")
    run("repo", "index", repository, succeeds=False)
    assert lock.read_text() == "user-owned lock"
    lock.unlink()
    before = snapshot(root)
    with read_lock(packages / "a.zip", share=0):
        run("repo", "index", repository, "--dry-run", succeeds=False)
    assert snapshot(root) == before
    # Force a different candidate while an existing catalog denies deletion.
    index.write_bytes(prefix + suffix)
    old = index.read_bytes()
    with read_lock(index):
        run("repo", "index", repository, succeeds=False)
    assert index.read_bytes() == old
    assert not candidate.exists() and not lock.exists()
    run("repo", "index", repository)
    assert index.read_bytes() == rebuilt
    # Distinct unsigned identities exercise the exact catalog capacity.
    held_packages = root / "held-packages"
    packages.rename(held_packages)
    packages.mkdir()
    for number in range(96):
        with zipfile.ZipFile(
            packages / f"fixture-{number:03d}.zip", "w"
        ) as package:
            package.writestr(
                ".wpm/package.txt",
                f"name=capacity-{number:03d}\nversion=1.0.0\narch=any\n",
            )
    run(
        "repo",
        "index",
        repository,
        "--allow-unsigned",
        "--dry-run",
        readonly=True,
    )
    with zipfile.ZipFile(packages / "fixture-096.zip", "w") as package:
        package.writestr(
            ".wpm/package.txt", "name=capacity-096\nversion=1.0.0\narch=any\n"
        )
    run("repo", "index", repository, "--allow-unsigned", succeeds=False)
    assert index.read_bytes() == rebuilt
    for archive in packages.iterdir():
        archive.unlink()
    packages.rmdir()
    held_packages.rename(packages)
    saved = repository / "saved-packages"
    packages.rename(saved)
    if junction(packages, saved):
        try:
            run("repo", "index", repository, "--dry-run", succeeds=False)
        finally:
            packages.rmdir()
    saved.rename(packages)
    for arguments in (
        ("repo", "index"),
        ("repo", "index", repository, "extra"),
        ("repo", "index", repository, "--sign", private),
        ("repo", "index", repository, "--index-key", public),
        ("repo", "index", repository, "--offline"),
    ):
        run(*arguments, succeeds=False, readonly=True)

print(
    "PASS: deterministic rebuild, preserved catalog bytes, guarded publication"
)
