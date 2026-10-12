"""Verify read-only capabilities with complete filesystem snapshots."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())


def snapshot(root):
    result = {}
    for path in sorted(root.rglob("*")):
        stat = path.stat()
        result[str(path.relative_to(root))] = (
            "directory"
            if path.is_dir()
            else hashlib.sha256(path.read_bytes()).hexdigest(),
            stat.st_mtime_ns,
        )
    return result


with tempfile.TemporaryDirectory(prefix="wpm-dry-run-") as directory:
    root = Path(directory)
    data = root / "data"
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    for phase in ("install", "remove"):
        (meta / f"{phase}.cmd").write_text(
            f'@echo off\necho forbidden>"{root / "script-ran"}"\nexit /b 0\n'
        )
    (source / "payload").write_text("immutable payload")
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

    def run(*arguments, expected=0):
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="y\n",
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == (expected == 0), result.stdout
        return result.stdout

    for version in ("1.0.0", "2.0.0"):
        (meta / "package.txt").write_text(
            f"name=dry-test\nversion={version}\narch=any\ndebug=false\n"
        )
        run("build", source, root)
    old = root / "dry-test-any-1.0.0.zip"
    new = root / "dry-test-any-2.0.0.zip"
    shutil.rmtree(data)
    before = snapshot(root)
    text = run("install", new, "--allow-unsigned", "--dry-run", "--verbose")
    assert "Plan digest:" in text and "dry-run" in text, text
    assert snapshot(root) == before
    assert not data.exists()

    # Exercise local repository and retained-record selection with no cached
    # index/package: dry-run must consume the source directly, not prepare it.
    run("repo", "add", root, "--priority", "1000")
    (root / "index.json").write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [
                    {
                        "name": "dry-test",
                        "version": "2.0.0",
                        "arch": "any",
                        "url": new.name,
                    }
                ],
            }
        )
    )
    shutil.copyfile(old, data / "packages" / old.name)
    for command in (
        ("install", "dry-test", "--allow-unsigned"),
        ("remove", old.stem),
        ("upgrade", "dry-test", "--allow-unsigned"),
        ("upgrade", "--all", "--allow-unsigned"),
    ):
        before = snapshot(root)
        options = () if command[0] == "remove" else ("--offline",)
        text = run(*command, "--dry-run", *options)
        assert "Plan digest:" in text and "dry-run" in text, text
        assert snapshot(root) == before, command
    # Self-upgrade is planned but cannot launch its deferred completion child.
    (meta / "package.txt").write_text(
        "name=wpm\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    run("build", source, root)
    shutil.copyfile(
        root / "wpm-any-1.0.0.zip", data / "packages" / "wpm-any-1.0.0.zip"
    )
    (meta / "package.txt").write_text(
        "name=wpm\nversion=2.0.0\narch=any\ndebug=false\n"
    )
    run("build", source, root)
    index = json.loads((root / "index.json").read_text())
    index["packages"].append(
        {
            "name": "wpm",
            "version": "2.0.0",
            "arch": "any",
            "url": "wpm-any-2.0.0.zip",
        }
    )
    (root / "index.json").write_text(json.dumps(index))
    before = snapshot(root)
    text = run("upgrade", "wpm", "--allow-unsigned", "--dry-run", "--offline")
    assert "scheduled after this process exits" in text, text
    assert snapshot(root) == before
    assert not (root / "script-ran").exists()

    # External log locations and malformed/unsupported combinations also must
    # remain unchanged. Environment settings cannot grant write capability.
    env["WPM_LOG_FILE"] = str(root / "external" / "never-created.log")
    for command in (
        ("install", new, "--allow-unsigned", "--yes"),
        ("install", new, "--allow-unsigned", "--confirm"),
        ("build", source),
        ("trust", "add", new),
        ("update",),
        ("repo", "update"),
        ("upgrade", "--all", "dry-test"),
        ("install", "missing-package", "--offline"),
        ("install", new),
        ("remove", "missing"),
        (
            "--color",
            "never",
            "--complete-self-upgrade",
            new,
            "0",
            "2.0.0",
            "any",
            "1.0.0",
            "1",
            root / "internal.log",
        ),
        (
            "--verbose",
            "--complete-self-upgrade",
            new,
            "0",
            "2.0.0",
            "any",
            "1.0.0",
            "1",
            root / "internal-verbose.log",
        ),
    ):
        before = snapshot(root)
        run(*command, "--dry-run", expected=1)
        assert snapshot(root) == before, command

    web = "https://github.com/example/dry-run/releases/latest/download"
    run("repo", "add", web, "--priority", "2000")
    value = 2166136261
    for byte in web.encode():
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    cache = data / "cache" / "repositories"
    cache.mkdir(parents=True, exist_ok=True)
    (cache / f"{value:08x}.json").write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [
                    {
                        "name": "web-test",
                        "version": "1.0.0",
                        "arch": "any",
                        "url": "package.zip",
                    }
                ],
            }
        )
    )
    before = snapshot(root)
    text = run(
        "install",
        "web-test",
        "--dry-run",
        "--offline",
        "--allow-unsigned",
        expected=1,
    )
    assert "already cached" in text, text
    assert snapshot(root) == before

    private = root / "signing.private"
    public = root / "signing.public"
    run("keygen", private, public)
    run("build", source, root, "--sign", private)
    signed = root / "wpm-any-2.0.0.zip"
    before = snapshot(root)
    run("install", signed, "--dry-run", expected=1)
    assert snapshot(root) == before
    run("trust", "add", public)
    before = snapshot(root)
    run("install", signed, "--dry-run")
    assert snapshot(root) == before
    key_id = next((data / "trust").glob("*.pub")).stem
    run("trust", "revoke", key_id)
    before = snapshot(root)
    run("install", signed, "--dry-run", expected=1)
    assert snapshot(root) == before

    damaged = root / "damaged.zip"
    with zipfile.ZipFile(new) as package:
        with zipfile.ZipFile(damaged, "w") as altered:
            for item in package.infolist():
                contents = package.read(item)
                if item.filename == "payload":
                    contents = b"tampered"
                altered.writestr(item, contents)
    before = snapshot(root)
    run("install", damaged, "--allow-unsigned", "--dry-run", expected=1)
    assert snapshot(root) == before

print(
    "PASS: dry-run plans, errors, local sources, "
    "and complete no-write snapshots"
)
