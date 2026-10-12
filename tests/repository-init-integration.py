"""Verify fresh repository initialization, dry-run, and overwrite protection."""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import tempfile

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


with tempfile.TemporaryDirectory(prefix="wpm-repo-init-") as directory:
    root = Path(directory)
    data = root / "data"
    repository = root / "published repository"
    env = dict(
        os.environ,
        WPM_DATA_DIR=str(data),
        WPM_LOG_FILE=str(root / "external.log"),
        WPM_LOG_DIR=str(root / "external-logs"),
    )

    def run(*arguments, succeeds=True, readonly=False):
        before = snapshot(root)
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            cwd=root,
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
        ("repo", "init", repository, "--dry-run"),
        ("repo", "init", "--dry-run", repository),
        ("--dry-run", "repo", "init", repository),
        ("repo", "init", "relative preview", "--dry-run", "--verbose"),
    ):
        text = run(*arguments, readonly=True)
        assert "initialization planned" in text and "schema 1" in text, text
        assert not data.exists()
        assert not repository.exists()
        assert not (root / "external.log").exists()
    text = run("repo", "init", repository)
    assert "repository initialized" in text, text
    assert json.loads((repository / "index.json").read_text()) == {
        "version": 1,
        "packages": [],
    }
    assert (repository / "packages").is_dir()
    assert (repository / "keys").is_dir()
    assert not (repository / ".index.json.new").exists()
    assert str(repository) not in run("repo", "list")
    before = snapshot(repository)
    text = run("repo", "init", repository, succeeds=False)
    assert "does not overwrite" in text, text
    assert snapshot(repository) == before
    run("repo", "init", repository, "--dry-run", succeeds=False, readonly=True)
    run("repo", "add", repository)
    run("update", "--offline")
    assert snapshot(repository) == before
    run("repo", "init", "relative repository")
    assert (root / "relative repository" / "index.json").exists()

    collision = root / "candidate collision"
    collision.mkdir()
    candidate = collision / ".index.json.new"
    candidate.write_text("user-owned candidate must survive")
    before = snapshot(collision)
    run("repo", "init", collision, succeeds=False)
    assert snapshot(collision) == before
    for entry in ("packages", "keys"):
        target = root / f"conflicting-{entry}"
        target.mkdir()
        (target / entry).write_text("user file must survive")
        before = snapshot(target)
        run("repo", "init", target, succeeds=False)
        assert snapshot(target) == before
    # A junction fixture uses only directories owned by this temporary root.
    # It avoids shell path expansion and never removes the junction target.
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
    kernel.DeviceIoControl.argtypes = [
        wintypes.HANDLE,
        wintypes.DWORD,
        ctypes.c_void_p,
        wintypes.DWORD,
        ctypes.c_void_p,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
        ctypes.c_void_p,
    ]
    kernel.DeviceIoControl.restype = wintypes.BOOL

    def junction(link, target):
        link.mkdir()
        substitute = ("\\??\\" + str(target)).encode("utf-16-le")
        display = str(target).encode("utf-16-le")
        paths = substitute + b"\0\0" + display + b"\0\0"
        payload = (
            struct.pack(
                "<IHHHHHH",
                0xA0000003,
                len(paths) + 8,
                0,
                0,
                len(substitute),
                len(substitute) + 2,
                len(display),
            )
            + paths
        )
        buffer = ctypes.create_string_buffer(payload)
        returned = wintypes.DWORD()
        handle = kernel.CreateFileW(
            str(link), 0x40000000, 3, None, 3, 0x02200000, None
        )
        if handle == wintypes.HANDLE(-1).value:
            link.rmdir()
            return False
        try:
            created = kernel.DeviceIoControl(
                handle,
                0x000900A4,
                buffer,
                len(payload),
                None,
                0,
                ctypes.byref(returned),
                None,
            )
        finally:
            kernel.CloseHandle(handle)
        if not created:
            link.rmdir()
        return bool(created)

    junction_target = root / "junction-target"
    junction_target.mkdir()
    for link, target in (
        (root / "root-junction", root / "root-junction"),
        (root / "layout-junction" / "packages", root / "layout-junction"),
    ):
        if link.parent != root:
            link.parent.mkdir()
        if junction(link, junction_target):
            try:
                before = snapshot(junction_target)
                text = run("repo", "init", target, succeeds=False)
                assert "reparse points" in text, text
                assert snapshot(junction_target) == before
            finally:
                link.rmdir()
        else:
            print("SKIP: fixture filesystem cannot create a directory junction")

    target_file = root / "target-file"
    target_file.write_text("unchanged")
    run("repo", "init", target_file, succeeds=False)
    assert target_file.read_text() == "unchanged"
    missing_parent = root / "missing-parent" / "repository"
    text = run("repo", "init", missing_parent, succeeds=False)
    assert "parent directory must exist" in text, text
    assert not missing_parent.parent.exists()
    for arguments in (
        ("--dry-run",),
        ("unknown-command",),
        ("--not-an-option", "repo", "init", repository),
        ("--dry-run", "build", root),
        ("--dry-run", "repo", "add", repository),
        ("repo", "init"),
        ("repo", "init", repository, "extra"),
        ("repo", "init", repository, "--priority", "1"),
        ("repo", "init", repository, "--offline"),
        ("repo", "init", repository, "--links"),
        ("repo", "list", "--dry-run"),
        ("repo", "add", repository, "--dry-run"),
    ):
        text = run(*arguments, succeeds=False, readonly=True)
        assert "Usage:" in text, text
    for target in ("https://example.invalid", "file:///C:/repository"):
        text = run(
            "repo", "init", target, "--dry-run", succeeds=False, readonly=True
        )
        assert "filesystem directory" in text, text

print("PASS: repository init, dry-run, relative paths and overwrite protection")
