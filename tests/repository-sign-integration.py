"""Catalog signer authority, exact bytes, atomic writes, and dry-run."""

import argparse
import hashlib
import json
import os
import shutil
import stat
from contextlib import contextmanager
from pathlib import Path
import subprocess
import tempfile

from repository_test_helpers import read_lock

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
            path.stat().st_file_attributes,
        )
        for path in sorted(root.rglob("*"))
    }


@contextmanager
def readonly_files(directory):
    """Set Windows file read-only attributes and restore them for cleanup."""
    modes = {
        path: path.stat().st_mode
        for path in directory.rglob("*")
        if path.is_file()
    }
    try:
        for path in modes:
            path.chmod(stat.S_IREAD)
            assert path.stat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY
        yield
    finally:
        for path, mode in modes.items():
            path.chmod(mode)


with tempfile.TemporaryDirectory(prefix="wpm-repository-sign-") as directory:
    root = Path(directory)
    data = root / "data"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

    def run(*arguments, succeeds=True, readonly=False, script=False):
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
        assert (root / "script-ran").exists() == script
        return result.stdout

    private, public = root / "package.private", root / "package.public"
    index_private, index_public = root / "index.private", root / "index.public"
    wrong_private, wrong_public = root / "wrong.private", root / "wrong.public"
    for pair in (
        (private, public),
        (index_private, index_public),
        (wrong_private, wrong_public),
    ):
        run("keygen", *pair)
    run("trust", "add", public)
    repository = root / "repository"
    empty = root / "empty-repository"
    run("repo", "init", repository)
    run("repo", "init", empty)
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    (meta / "package.txt").write_text(
        "name=signed-catalog\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    (meta / "install.cmd").write_text(
        f'@echo off\necho ran>"{root / "script-ran"}"\nexit /b 0\n'
    )
    (source / "payload").write_text("catalog signed separately")
    archives = root / "archives"
    archives.mkdir()
    run("build", source, archives, "--sign", private)
    run(
        "repo",
        "add-package",
        repository,
        archives / "signed-catalog-any-1.0.0.zip",
    )
    original = (repository / "index.json").read_bytes()

    def sign(
        *options,
        succeeds=True,
        readonly=False,
        target=repository,
        selected_private=index_private,
        selected_public=index_public,
    ):
        return run(
            "repo",
            "sign",
            target,
            "--sign",
            selected_private,
            "--index-key",
            selected_public,
            *options,
            succeeds=succeeds,
            readonly=readonly,
        )

    def verify(*options, succeeds=True):
        return run(
            "repo",
            "verify",
            repository,
            "--index-key",
            index_public,
            *options,
            succeeds=succeeds,
            readonly=True,
        )

    verify(succeeds=False)
    run("repo", "verify", repository, readonly=True)
    fake = root / "not-a-private-key"
    fake.write_text("private key bytes must not be read during dry-run")
    env["WPM_DATA_DIR"] = str(root / "absent-data")
    env["WPM_LOG_FILE"] = str(root / "absent-external.log")
    env["WPM_LOG_DIR"] = str(root / "absent-external-logs")
    text = sign("--dry-run", target=empty, selected_private=fake, readonly=True)
    assert "Private key bytes are not read" in text, text
    assert not (root / "absent-data").exists()
    assert not (empty / "index.sig").exists()
    env["WPM_DATA_DIR"] = str(data)
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR"):
        env.pop(variable)
    public_original = index_public.read_bytes()
    index_public.write_bytes(
        public_original.replace(b"wpm-public-key-v1", b"wpm-public-key-v10")
    )
    sign("--dry-run", succeeds=False, readonly=True)
    index_public.write_bytes(public_original)
    for ending in (b"\n", b"\r\n"):
        index_public.write_bytes(public_original.replace(b"\n", ending))
        sign("--dry-run", readonly=True)
    index_public.write_bytes(public_original)
    private_original = index_private.read_bytes()
    index_private.write_bytes(
        private_original.replace(b"wpm-private-key-v1", b"wpm-private-key-v10")
    )
    sign(succeeds=False)
    index_private.write_bytes(private_original)
    sign("--dry-run", readonly=True)
    before = snapshot(root)
    with read_lock(index_private, share=0):
        sign("--dry-run")
    assert snapshot(root) == before
    sign(selected_private=wrong_private, succeeds=False)
    assert not (repository / "index.sig").exists()
    trust_before = snapshot(data / "trust")
    sign()
    assert snapshot(data / "trust") == trust_before
    assert (repository / "index.json").read_bytes() == original
    signature = repository / "index.sig"
    signature_before = signature.read_bytes()
    descriptor = json.loads(signature_before)
    assert descriptor["purpose"] == "repository-index", descriptor
    assert "secret-key" not in signature_before.decode()
    verify()
    run(
        "repo",
        "verify",
        repository,
        "--index-key",
        public,
        succeeds=False,
        readonly=True,
    )  # Package trust does not authorize index B.
    sign(
        succeeds=False
    )  # Existing signature is immutable without authorization.
    assert signature.read_bytes() == signature_before
    sign("--replace-signature", "--dry-run", readonly=True)
    candidate = repository / ".index.sig.new"
    candidate.write_text("user-owned candidate")
    sign("--replace-signature", succeeds=False)
    assert candidate.read_text() == "user-owned candidate"
    assert signature.read_bytes() == signature_before
    candidate.unlink()
    lock = repository / ".wpm-publish.lock"
    lock.write_text("user-owned lock")
    sign("--replace-signature", succeeds=False)
    assert lock.read_text() == "user-owned lock"
    lock.unlink()
    (repository / "index.json").write_bytes(original + b"\n")
    verify(succeeds=False)  # A whitespace change is a byte change.
    sign("--replace-signature")
    verify()
    current = signature.read_bytes()
    signature.write_text('{"version":1,"purpose":"repository-index"}')
    verify(succeeds=False)
    signature.write_bytes(current)
    for changed in (
        current.replace(b'"version": 1', b'"version": 10'),
        current.replace(
            b'"purpose": "repository-index"', b'"purpose": "package-index"'
        ),
        current.replace(
            b'"algorithm": "ed25519"', b'"algorithm": "unsupported"'
        ),
        current[:-2],
        current + b"\0",
    ):
        signature.write_bytes(changed)
        verify(succeeds=False)
    signature.write_bytes(current)
    # Duplicate required fields cannot choose competing signer declarations.
    signature.write_bytes(
        current.replace(b'"version": 1,', b'"version": 1, "version": 1,')
    )
    verify(succeeds=False)
    signature.write_bytes(current)
    for flag, value in (
        ("--sign", index_private),
        ("--index-key", index_public),
    ):
        run(
            "repo",
            "sign",
            repository,
            "--sign",
            index_private,
            "--index-key",
            index_public,
            flag,
            value,
            succeeds=False,
            readonly=True,
        )
    # Failure to replace a locked destination leaves the old signature intact.
    with read_lock(signature):
        sign("--replace-signature", succeeds=False)
    assert signature.read_bytes() == current
    assert not candidate.exists() and not lock.exists()
    private_in_root = repository / "private-input"
    private_in_root.write_bytes(index_private.read_bytes())
    sign(
        "--replace-signature", selected_private=private_in_root, succeeds=False
    )
    private_in_root.unlink()
    for arguments in (
        ("repo", "sign", repository),
        ("repo", "init", repository, "--index-key", index_public),
        ("repo", "verify", repository, "--sign", index_private),
        ("repo", "list", "--replace-signature"),
    ):
        run(*arguments, succeeds=False, readonly=True)
    verify()
    # Complete copied layout verifies with the independent index key, then
    # legacy consumption installs from relative package locators unchanged.
    copied = root / "copied-distribution"
    shutil.copytree(repository, copied, copy_function=shutil.copy2)
    with readonly_files(copied):
        try:
            with (copied / "index.json").open("r+b"):
                raise AssertionError("read-only catalog allowed write access")
        except PermissionError:
            pass
        copied_before = snapshot(copied)
        env["WPM_DATA_DIR"] = str(root / "consumer-data")
        run("trust", "add", public)
        run(
            "repo", "verify", copied, "--index-key", index_public, readonly=True
        )
        run("repo", "add", copied, "--priority", "1000")
        run("update", "--offline")
        run("install", "signed-catalog", "--offline", script=True)
        assert snapshot(copied) == copied_before
        assert (
            root / "consumer-data" / "packages" / "signed-catalog-any-1.0.0.zip"
        ).exists()
        (root / "script-ran").unlink()
    env["WPM_DATA_DIR"] = str(data)
    run("trust", "revoke", next((data / "trust").glob("*.pub")).stem)
    verify(succeeds=False)
    verify("--allow-unsigned", succeeds=False)

print(
    "PASS: explicit index authority, exact signatures, atomic writes, dry-run"
)
