"""Inspect real journals and current archives without mutation or replay."""

import argparse
import copy
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
    def digest(path):
        if path.is_dir():
            return None
        try:
            return hashlib.sha256(path.read_bytes()).hexdigest()
        except PermissionError:
            return "locked"

    return {
        str(path.relative_to(root)): (
            digest(path),
            path.stat().st_mtime_ns,
            path.stat().st_file_attributes,
        )
        for path in sorted(root.rglob("*"))
    }


with tempfile.TemporaryDirectory(prefix="wpm-recovery-record-") as directory:
    root = Path(directory)
    data = root / "data"
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    marker = root / "script-marker"
    (meta / "package.txt").write_text(
        "name=record-test\nversion=1.0.0\narch=any\ndebug=false\n",
        encoding="utf-8",
    )
    (meta / "install.cmd").write_text(
        f'@echo off\necho changed>"{marker}"\nexit /b 37\n',
        encoding="utf-8",
    )
    (source / "payload.txt").write_text("payload", encoding="utf-8")
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    env["WPM_LOG_FILE"] = str(root / "query-must-not-log.txt")

    def run(*arguments, succeeds=True, input_text="", readonly=False):
        before = snapshot(root) if readonly else None
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input=input_text,
            text=True,
            encoding="utf-8",
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, (
            result.stdout,
            result.stderr,
        )
        if readonly:
            assert snapshot(root) == before
        return result

    def query(*arguments, succeeds=True, envelope=True):
        selector = ("--output", "json") if envelope else ("--json",)
        result = run(
            "recovery",
            *arguments,
            *selector,
            succeeds=succeeds,
            readonly=True,
        )
        assert not result.stderr, result.stderr
        assert "\x1b" not in result.stdout
        value = json.loads(result.stdout)
        if envelope:
            assert value["schema"] == "wpm.output.v1"
            assert value["exitCode"] == (0 if succeeds else 1)
            if value["result"] is None:
                return value
            assert value["resultSchema"] == "wpm.recovery-query.v1"
            value = value["result"]
        assert value["schema"] == "wpm.recovery-query.v1"
        assert value["exitCode"] == (0 if succeeds else 1)
        assert value["retrySupported"] is False
        return value

    # Absent state stays absent; invalid IDs never become path arguments.
    assert query("records")["records"] == []
    assert not data.exists()
    for invalid in ("..", "A" * 32, "a" * 31, "a" * 33, "a" * 32 + ".json"):
        value = query("inspect", invalid, succeeds=False)
        assert value["result"] is None
    assert not data.exists()
    for operands in (
        ("inspect",),
        ("records", "extra"),
        ("records", "--archive", "file.zip"),
        ("records", "--allow-unsigned"),
    ):
        assert query(*operands, succeeds=False)["result"] is None
    query("inspect", "a" * 32, "--allow-unsigned", succeeds=False)

    assert run("build", source, root).returncode == 0
    archive = root / "record-test-any-1.0.0.zip"
    run(
        "install",
        archive,
        "--allow-unsigned",
        "--confirm",
        succeeds=False,
        input_text="y\n",
    )
    assert marker.exists()
    marker.unlink()
    record_path = next((data / "recovery").glob("*.json"))
    record_id = record_path.stem
    original = record_path.read_bytes()
    original_value = json.loads(original)
    listed = query("records")
    assert len(listed["records"]) == 1
    record = listed["records"][0]
    assert record["status"] == "failed"
    assert record["scriptExitCode"] == 37
    assert record["inputStatus"] == "not-checked"
    assert record["authenticated"] is False
    assert record["retryEligible"] is False
    assert query("records", envelope=False) == listed
    assert query("records", "--name", "absent")["records"] == []
    assert query("records", "--arch", "ANY") == query("records")
    assert not marker.exists()

    # Ordering remains deterministic, while a source-wide bound failure cannot
    # become success merely because a display filter excludes every row.
    additional = []
    for number in (1, 2):
        identity = f"{number:032x}"
        candidate = copy.deepcopy(original_value)
        candidate["operationId"] = identity
        candidate["controlledArtifacts"][0]["relativePath"] = (
            f"{identity}/input.zip"
        )
        path = record_path.parent / f"{identity}.json"
        path.write_text(json.dumps(candidate), encoding="utf-8")
        additional.append(path)
    ordered = query("records")["records"]
    assert [row["operationId"] for row in ordered] == sorted(
        row["operationId"] for row in ordered
    )
    assert query("records")["records"] == ordered
    limited = query("records", "--limit", "1", succeeds=False)
    assert limited["summary"]["limited"] is True
    query("records", "--limit", "1", "--name", "absent", succeeds=False)
    for path in additional:
        path.unlink()

    # A stored external path cannot authorize inspection of its contents.
    unchecked = query("inspect", record_id, succeeds=False)["records"][0]
    assert unchecked["inputStatus"] == "untrusted-or-invalid"
    retained = query("inspect", record_id, "--allow-unsigned")["records"][0]
    assert retained["retainedInputStatus"] == "validated-current-input"
    retained_path = Path(retained["retainedInputPath"])
    assert retained_path.read_bytes() == archive.read_bytes()
    moved = root / "moved-original.zip"
    archive.rename(moved)
    query("inspect", record_id, "--allow-unsigned")
    moved.rename(archive)
    retained_bytes = retained_path.read_bytes()
    retained_path.write_bytes(retained_bytes + b"tampered")
    damaged = query("inspect", record_id, "--allow-unsigned", succeeds=False)[
        "records"
    ][0]
    assert damaged["retainedInputStatus"] == "changed"
    # A good explicit archive does not conceal a corrupt recorded input.
    mismatch = query(
        "inspect",
        record_id,
        "--archive",
        archive,
        "--allow-unsigned",
        succeeds=False,
    )["records"][0]
    assert mismatch["inputStatus"] == "validated-current-input"
    assert mismatch["retainedInputStatus"] == "changed"
    retained_path.write_bytes(retained_bytes)
    retained_path.rename(root / "missing-retained.zip")
    assert (
        query("inspect", record_id, "--allow-unsigned", succeeds=False)[
            "records"
        ][0]["retainedInputStatus"]
        == "missing"
    )
    (root / "missing-retained.zip").rename(retained_path)
    validated = query(
        "inspect", record_id, "--archive", archive, "--allow-unsigned"
    )["records"][0]
    assert validated["inputStatus"] == "validated-current-input"
    assert validated["safeActions"] == ["inspect"]
    assert not marker.exists()
    assert (
        query(
            "inspect",
            record_id,
            "--archive",
            archive,
            "--allow-unsigned",
            envelope=False,
        )["records"][0]
        == validated
    )
    unsigned = query(
        "inspect", record_id, "--archive", archive, succeeds=False
    )["records"][0]
    assert unsigned["inputStatus"] == "untrusted-or-invalid"
    archive_bytes = archive.read_bytes()
    archive.write_bytes(archive_bytes + b"changed")
    changed = query(
        "inspect",
        record_id,
        "--archive",
        archive,
        "--allow-unsigned",
        succeeds=False,
    )["records"][0]
    assert changed["inputStatus"] == "changed"
    archive.write_bytes(archive_bytes)
    missing = query(
        "inspect",
        record_id,
        "--archive",
        root / "absent.zip",
        "--allow-unsigned",
        succeeds=False,
    )["records"][0]
    assert missing["inputStatus"] == "missing"
    for unsafe in (
        str(archive) + ":stream",
        str(archive) + ".",
        str(archive) + " ",
    ):
        assert (
            query(
                "inspect",
                record_id,
                "--archive",
                unsafe,
                "--allow-unsigned",
                succeeds=False,
            )["records"][0]["inputStatus"]
            == "unsafe-path"
        )

    # Inconsistent metadata, duplicate fields, invalid UTF-8, future schemas,
    # and oversized/truncated inputs are preserved and report source failure.
    malformed = []
    for field, value in (
        ("schema", "wpm.recovery.v2"),
        ("operationId", "b" * 32),
        ("created", "2026-02-30T00:00:00.000Z"),
        ("created", "9999-01-01T00:00:00.000Z"),
        ("retryEligible", True),
        ("scriptExitCode", 4294967296),
        ("scriptExitCode", -1),
        ("scriptExitCode", 1.5),
        ("safeActions", ["retry"]),
        ("controlledArtifacts", {}),
        ("completedPhases", "guessed"),
        ("failedPhase", "other"),
    ):
        candidate = copy.deepcopy(original_value)
        candidate[field] = value
        malformed.append(json.dumps(candidate).encode("utf-8"))
    malformed.extend(
        (
            original[:-4],
            b" " * 65537,
            b'{"schema":"wpm.recovery.v1",\xff}',
            original.replace(
                b'"operationId":',
                b'"operationId":"' + b"a" * 32 + b'","operationId":',
                1,
            ),
            original.replace(b'"name":', b'"name":"other","name":', 1),
            original.replace(b'"name":', b'"name":"other","\\u006eame":', 1),
        )
    )
    for content in malformed:
        record_path.write_bytes(content)
        value = query("records", succeeds=False)
        assert value["summary"]["invalidOrUnreadable"] == 1
        assert value["records"] == []
        query("inspect", record_id, succeeds=False)
        assert record_path.read_bytes() == content
    record_path.write_bytes(original)

    # Fields with matching digest but conflicting metadata still fail; encoded
    # credentials are filtered by the shared renderer rather than replayed.
    tampered = copy.deepcopy(original_value)
    tampered["package"]["name"] = "other"
    record_path.write_text(json.dumps(tampered), encoding="utf-8")
    assert (
        query(
            "inspect",
            record_id,
            "--archive",
            archive,
            "--allow-unsigned",
            succeeds=False,
        )["records"][0]["inputStatus"]
        == "identity-conflict"
    )
    tampered = copy.deepcopy(original_value)
    tampered["source"] = (
        "https://user:password@example.invalid/file?token=secret"
    )
    record_path.write_text(json.dumps(tampered), encoding="utf-8")
    redacted = json.dumps(query("records"))
    assert "password" not in redacted and "secret" not in redacted
    assert "example.invalid" in redacted
    record_path.write_bytes(original)

    with read_lock(record_path, share=0):
        value = query("records", succeeds=False)
        assert value["summary"]["invalidOrUnreadable"] == 1
    linked = root / "linked-record.json"
    os.link(record_path, linked)
    query("records", succeeds=False)
    linked.unlink()
    outside = root / "outside"
    outside.mkdir()
    saved = data / "saved-recovery"
    (data / "recovery").rename(saved)
    assert junction(data / "recovery", outside), "junction fixture unavailable"
    try:
        query("records", succeeds=False)
        assert not list(outside.iterdir())
    finally:
        (data / "recovery").rmdir()
        saved.rename(data / "recovery")

    # A current signature is rechecked against present trust, including revoke.
    private, public = root / "test.private", root / "test.public"
    run("keygen", private, public)
    run("trust", "add", public)
    run("build", source, root, "--sign", private)
    run("install", archive, "--confirm", succeeds=False, input_text="y\n")
    signed_id = next(
        path.stem
        for path in (data / "recovery").glob("*.json")
        if path.stem != record_id
    )
    query("inspect", signed_id, "--archive", archive)
    signed_bytes = archive.read_bytes()
    (meta / "install.cmd").write_text(
        "@echo off\nexit /b 0\n", encoding="utf-8"
    )
    run("build", source, root, "--sign", private)
    run("install", archive, "--confirm", input_text="y\n")
    before_remove = set((data / "recovery").glob("*.json"))
    run("remove", archive.stem, "--confirm", input_text="y\n")
    removal_path = (
        set((data / "recovery").glob("*.json")) - before_remove
    ).pop()
    removal = query("inspect", removal_path.stem, "--archive", archive)[
        "records"
    ][0]
    assert removal["operation"] == "remove"
    assert removal["oldVersion"] == "1.0.0"
    assert removal["newVersion"] is None
    # Preserve older null-version removal documents as incomplete identity;
    # never invent a version or synthesize safe retry from incomplete metadata.
    original_removal = removal_path.read_bytes()
    legacy_removal = json.loads(original_removal)
    legacy_removal["package"]["oldVersion"] = None
    removal_path.write_text(json.dumps(legacy_removal), encoding="utf-8")
    assert query("records")["complete"] is True
    assert (
        query(
            "inspect", removal_path.stem, "--archive", archive, succeeds=False
        )["records"][0]["inputStatus"]
        == "identity-incomplete"
    )
    removal_path.write_bytes(original_removal)
    archive.write_bytes(signed_bytes)
    signer = next((data / "trust").glob("*.pub")).stem
    run("trust", "revoke", signer)
    revoked = query("inspect", signed_id, "--archive", archive, succeeds=False)[
        "records"
    ][0]
    assert revoked["inputStatus"] == "untrusted-or-invalid"

    # A hostile but individually bounded directory cannot force unbounded
    # cumulative reads. These are unauthenticated parser fixtures, not execution
    # evidence; source exhaustion must remain non-success with all data intact.
    padded_paths = []
    for number in range(1000, 2050):
        identity = f"{number:032x}"
        candidate = copy.deepcopy(original_value)
        candidate["operationId"] = identity
        candidate["controlledArtifacts"][0]["relativePath"] = (
            f"{identity}/input.zip"
        )
        text = json.dumps(candidate).encode("utf-8")
        path = record_path.parent / f"{identity}.json"
        path.write_bytes(text + b" " * (65536 - len(text)))
        padded_paths.append(path)
    exhausted = query("records", "--limit", "4096", succeeds=False)
    assert exhausted["summary"]["limited"] is True
    assert exhausted["summary"]["invalidOrUnreadable"] == 0
    assert exhausted["complete"] is False
    for path in padded_paths:
        path.unlink()

print("PASS: bounded recovery admission, explicit revalidation, and no replay")
