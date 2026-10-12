"""Verify immutable publication, dry-run, collisions, and consumption."""

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


with tempfile.TemporaryDirectory(prefix="wpm-repo-publish-") as directory:
    root = Path(directory)
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    marker = root / "installed"
    (meta / "package.txt").write_text(
        "name=published-check\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    (meta / "install.cmd").write_text(
        f'@echo off\necho installed>"{marker}"\nexit /b 0\n'
    )
    (meta / "remove.cmd").write_text("@echo off\nexit /b 0\n")
    (source / "payload").write_text("immutable published payload")
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
        return result.stdout

    repository = root / "published repository"
    previous = root / "previous"
    archives = root / "archives"
    archives.mkdir()
    private, public = root / "private", root / "public"
    run("keygen", private, public)
    run("build", source, archives, "--sign", private)
    archive = archives / "published-check-any-1.0.0.zip"
    run("repo", "init", repository)
    run("repo", "init", previous)
    index = repository / "index.json"
    original = (
        '{"version":1.0,"extension":{"nested":[true,null,"keep"]},'
        '"packages": [\n], "repositories": ["previous"]}\n'
    )
    index.write_text(original)
    (repository / "previous").mkdir()
    (repository / "previous" / "index.json").write_bytes(
        (previous / "index.json").read_bytes()
    )

    # The author catalog can preserve external links without following them.
    def publish(succeeds=True, *options):
        return run(
            "repo",
            "add-package",
            repository,
            archive,
            *options,
            succeeds=succeeds,
            readonly="--dry-run" in options,
        )

    for options in (("--dry-run",), ("--allow-unsigned", "--dry-run")):
        publish(False, *options)
    assert not marker.exists()
    assert not (repository / ".wpm-publish.lock").exists()
    run("trust", "add", public)
    before_source = snapshot(source)
    original_archive = archive.read_bytes()
    digest = hashlib.blake2b(original_archive, digest_size=32).hexdigest()
    artifact = repository / "packages" / f"{digest}.zip"
    before_index = snapshot(repository)
    env["WPM_LOG_FILE"] = str(root / "external.log")
    env["WPM_LOG_DIR"] = str(root / "external-logs")
    text = publish(True, "--dry-run", "--verbose")
    assert "publication planned" in text and digest in text, text
    run("--dry-run", "repo", "add-package", repository, archive, readonly=True)
    run(
        "repo",
        "--verbose",
        "add-package",
        repository,
        "--dry-run",
        archive,
        readonly=True,
    )
    assert snapshot(repository) == before_index
    assert not (root / "external.log").exists()
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR"):
        env.pop(variable)
    # User-owned lock and index candidates must survive failed publication.
    for filename in (".wpm-publish.lock", ".index.json.new"):
        collision = repository / filename
        collision.write_text("user-owned collision")
        preserved = (collision.read_bytes(), collision.stat().st_mtime_ns)
        publish(False)
        assert (
            collision.read_bytes(),
            collision.stat().st_mtime_ns,
        ) == preserved
        assert index.read_text() == original
        assert not artifact.exists()
        collision.unlink()
    # An existing artifact can only be reused when its complete bytes match.
    artifact.write_text("different bytes at expected content-addressed path")
    preserved = (artifact.read_bytes(), artifact.stat().st_mtime_ns)
    publish(False)
    assert (artifact.read_bytes(), artifact.stat().st_mtime_ns) == preserved
    assert index.read_text() == original
    artifact.unlink()
    candidate = Path(str(artifact) + ".new")
    candidate.write_text("owned archive candidate")
    publish(False)
    assert candidate.read_text() == "owned archive candidate"
    assert not artifact.exists()
    candidate.unlink()
    artifact.write_bytes(archive.read_bytes())
    artifact_time = artifact.stat().st_mtime_ns
    text = publish()
    assert "published (1 packages)" in text, text
    assert artifact.stat().st_mtime_ns == artifact_time
    assert snapshot(source) == before_source
    assert not marker.exists()
    assert not list(repository.glob(".*.new"))
    assert not (repository / ".wpm-publish.lock").exists()
    catalog = json.loads(index.read_text())
    assert catalog["extension"] == {"nested": [True, None, "keep"]}
    assert catalog["repositories"] == ["previous"]
    assert catalog["packages"] == [
        {
            "name": "published-check",
            "version": "1.0.0",
            "arch": "any",
            "url": f"packages/{digest}.zip",
        }
    ]
    compact_entry = json.dumps(catalog["packages"][0], separators=(",", ":"))
    assert index.read_text() == original.replace(
        '"packages": [\n]', f'"packages": [\n{compact_entry}]'
    )
    index_before = (index.read_bytes(), index.stat().st_mtime_ns)
    publish(False)
    publish(False, "--dry-run")
    assert (index.read_bytes(), index.stat().st_mtime_ns) == index_before
    run("repo", "verify", repository, readonly=True)
    run("repo", "add", repository, "--priority", "1000")
    run("update", "--offline")
    run("install", "published-check", "--arch", "any", "--offline")
    assert marker.exists()
    # A second version requires a new identity and preserves the first entry.
    (meta / "package.txt").write_text(
        "name=published-check\nversion=1.0.1\narch=any\ndebug=false\n"
    )
    run("build", source, archives, "--sign", private)
    archive = archives / "published-check-any-1.0.1.zip"
    publish()
    assert len(json.loads(index.read_text())["packages"]) == 2
    assert artifact.read_bytes() == original_archive
    run("repo", "verify", repository, readonly=True)
    # Fresh copied publication and explicit unsigned opt-in.
    unsigned_repository = root / "unsigned repository"
    run("repo", "init", unsigned_repository)
    (meta / "signature.json").unlink()
    run("build", source, archives)
    run("repo", "add-package", unsigned_repository, archive, succeeds=False)
    unsigned_index = unsigned_repository / "index.json"
    preserved = (unsigned_index.read_bytes(), unsigned_index.stat().st_mtime_ns)
    unsigned_digest = hashlib.blake2b(archive.read_bytes(), digest_size=32)
    unsigned_artifact = (
        unsigned_repository / "packages" / f"{unsigned_digest.hexdigest()}.zip"
    )
    with read_lock(unsigned_index):
        run(
            "repo",
            "add-package",
            unsigned_repository,
            archive,
            "--allow-unsigned",
            succeeds=False,
        )
        assert (
            unsigned_index.read_bytes(),
            unsigned_index.stat().st_mtime_ns,
        ) == preserved
    assert unsigned_artifact.read_bytes() == archive.read_bytes()
    artifact_time = unsigned_artifact.stat().st_mtime_ns
    assert not (unsigned_repository / ".index.json.new").exists()
    assert not (unsigned_repository / ".wpm-publish.lock").exists()
    run("repo", "add-package", unsigned_repository, archive, "--allow-unsigned")
    assert unsigned_artifact.stat().st_mtime_ns == artifact_time
    run(
        "repo", "verify", unsigned_repository, "--allow-unsigned", readonly=True
    )
    for options in (
        ("--links",),
        ("--offline",),
        ("--priority", "1"),
        ("--allow-insecure-http",),
        ("extra",),
    ):
        run(
            "repo",
            "add-package",
            repository,
            archive,
            *options,
            succeeds=False,
            readonly=True,
        )
    link = root / "root junction"
    if junction(link, repository):
        try:
            run(
                "repo",
                "add-package",
                link,
                archive,
                "--dry-run",
                succeeds=False,
                readonly=True,
            )
        finally:
            link.rmdir()
    else:
        print("SKIP: fixture filesystem cannot create a directory junction")

print("PASS: validated immutable publication, dry-run, and legacy consumption")
