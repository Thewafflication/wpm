"""Verify local media failures preserve caches and report safe retry actions."""

import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-local-media-") as directory:
    root = Path(directory)
    data = root / "data"
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    repository = root / "repository"
    repository.mkdir()
    marker = root / "installed"
    (meta / "package.txt").write_text(
        "name=media-check\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    (meta / "install.cmd").write_text(
        f'@echo off\necho installed>"{marker}"\nexit /b 0\n'
    )
    (source / "payload").write_text("source must remain intact")
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

    def run(*arguments, succeeds=True):
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="",
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        return result.stdout

    def cache_snapshot():
        return {
            str(path.relative_to(data)): path.read_bytes()
            for path in (data / "cache").rglob("*")
            if path.is_file()
        }

    private = root / "private"
    public = root / "public"
    run("keygen", private, public)
    run("trust", "add", public)
    run("build", source, repository, "--sign", private)
    archive = repository / "media-check-any-1.0.0.zip"
    index = repository / "index.json"
    index.write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [
                    {
                        "name": "media-check",
                        "version": "1.0.0",
                        "arch": "any",
                        "url": archive.name,
                    }
                ],
            }
        )
    )
    run("repo", "add", repository, "--priority", "1000")
    run("update", "--offline")
    before = cache_snapshot()

    # A source lock prevents refresh; an earlier complete cache survives.
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        ctypes.c_void_p,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.HANDLE,
    ]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    handle = kernel.CreateFileW(str(index), 0x80000000, 0, None, 3, 0, None)
    assert handle != wintypes.HANDLE(-1).value, ctypes.get_last_error()
    try:
        text = run("update", "--offline", succeeds=False)
        assert "open source" in text and "Retry:" in text, text
        assert "previous cache was retained" in text, text
        assert cache_snapshot() == before
    finally:
        kernel.CloseHandle(handle)

    # Invalid local catalogs fail closed and cannot replace a previous cache.
    original_index = index.read_bytes()
    index.write_text('{"version":10,"packages":[]}')
    try:
        text = run("update", "--offline", succeeds=False)
        assert "previous cache was retained" in text, text
        assert cache_snapshot() == before
        assert not list((data / "cache").rglob("*.candidate*"))
    finally:
        index.write_bytes(original_index)

    # Removed media cannot silently fall back to its old index for installation.
    disconnected = root / "disconnected"
    repository.rename(disconnected)
    try:
        text = run("install", "media-check", "--offline", succeeds=False)
        assert "configured path" in text and "Retry:" in text, text
        assert cache_snapshot() == before
        assert not marker.exists()
    finally:
        disconnected.rename(repository)

    # An index is still available, but its selected archive has disappeared.
    absent_archive = root / archive.name
    archive.rename(absent_archive)
    try:
        text = run("install", "media-check", "--offline", succeeds=False)
        assert str(archive) in text and "Retry:" in text, text
        assert "invalid package URL" not in text, text
        text = run(
            "install", "media-check", "--offline", "--confirm", succeeds=False
        )
        assert "selected local package is unavailable" in text, text
        assert "recompute the plan" in text, text
        assert "invalid package URL" not in text, text
        assert not marker.exists()
        assert not list((data / "cache").rglob("*.download"))
    finally:
        absent_archive.rename(archive)

    originals = {path.name: path.read_bytes() for path in (index, archive)}
    for path in (index, archive):
        path.chmod(stat.S_IREAD)
    try:
        run("update", "--offline")
        run("install", "media-check", "--offline")
        assert marker.exists()
        assert originals == {
            path.name: path.read_bytes() for path in (index, archive)
        }
    finally:
        for path in (index, archive):
            path.chmod(stat.S_IWRITE)

print("PASS: locked/missing media, cache retention, signed read-only install")
