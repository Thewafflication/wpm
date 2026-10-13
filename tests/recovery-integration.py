"""Inspect a real failed upgrade without mutation or script replay."""

import argparse
import hashlib
import json
import os
import shutil
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
        text = run(
            "recovery",
            "list",
            "--json",
            *arguments,
            succeeds=succeeds,
            readonly=True,
        )
        assert text.endswith("\n") and "\x1b" not in text, text
        value = json.loads(text)
        assert value["schema"] == "wpm.recovery-list.v1", value
        assert value["exit_code"] == (0 if succeeds else 1), value
        assert value["authenticated"] is False, value
        assert value["external_state_known"] is False, value
        assert value["automatic_retry_supported"] is False, value
        return value

    value = machine()
    assert value["records"] == [] and value["status"] == "complete", value
    assert value["summary"]["limit"] == 128, value
    assert "filters" not in value and "selection" not in value
    empty_filtered = machine("--name", "absent", "--arch", "X64")
    assert empty_filtered["filters"] == {"name": "absent", "arch": "X64"}
    assert empty_filtered["selection"] == {"examined": 0, "filtered_out": 0}
    for options in (
        ("--name",),
        ("--arch",),
        ("--name", ""),
        ("--name", "x" * 128),
        ("--name", "bad\tname"),
        ("--name", "bad\x7fname"),
        ("--arch", "unsupported"),
        ("--name", "first", "--name", "second"),
        ("--arch", "x64", "--arch", "x86"),
        ("--name", "--json"),
    ):
        run("recovery", "list", *options, succeeds=False, readonly=True)
    assert not data.exists() and not (root / "external.log").exists()
    run("recovery", "--json", "--verbose", succeeds=False, readonly=True)
    for arguments in (
        ("recovery",),
        ("recovery", "list"),
        ("recovery", "list", "--limit", "04096"),
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
    value = machine()
    record = value["records"][0]
    assert record["record"] == audit.name, value
    assert record["name"] == "recovery-check" and record["exit_code"] == 7
    assert record["phase"] == "install-script", value
    assert value["summary"]["serialization_issues"] == 0, value
    value = json.loads(
        run("--color", "always", "recovery", "--json", readonly=True)
    )
    assert value["complete"] is True, value
    original = audit.read_bytes()
    selected = machine("--name", "RECOVERY-CHECK", "--arch", "ANY")
    assert selected["records"] == [record]
    assert selected["selection"] == {"examined": 1, "filtered_out": 0}
    absent = machine("--name", "absent")
    assert absent["records"] == [] and absent["selection"]["filtered_out"] == 1
    assert machine("--arch", "x64")["records"] == []
    other = audit.parent / "other.upgrade-failed.txt"
    other.write_bytes(
        original.replace(b"name=recovery-check", b"name=other").replace(
            b"arch=any", b"arch=x64"
        )
    )
    selected = machine("--arch", "X64")
    assert [item["name"] for item in selected["records"]] == ["other"]
    assert selected["selection"] == {"examined": 2, "filtered_out": 1}
    human = run("recovery", "list", "--name", "recovery-check", readonly=True)
    assert "1 recorded upgrade failures" in human and "filtered out=1" in human
    other.write_bytes(b"malformed")
    selected = machine("--name", "recovery-check", succeeds=False)
    assert selected["records"] == [record]
    assert selected["summary"]["invalid_or_unreadable"] == 1
    other.unlink()
    protected_name = "token=DO_NOT_LEAK_FILTER"
    audit.write_bytes(
        original.replace(
            b"name=recovery-check", b"name=" + protected_name.encode()
        )
    )
    selected = machine("--name", protected_name)
    assert len(selected["records"]) == 1
    assert "DO_NOT_LEAK_FILTER" not in json.dumps(selected)
    audit.write_bytes(original)
    secret = b"DO_NOT_LEAK_AUDIT_CREDENTIAL"
    audit.write_bytes(
        original.replace(
            b"name=recovery-check",
            b"name=https://user:" + secret + b"@example.invalid",
        )
    )
    text = run("recovery", "list", readonly=True)
    assert secret.decode() not in text
    assert secret.decode() not in json.dumps(machine())
    label = '\u540d\U0001f4a1 "quoted"\\path\ttail'
    audit.write_bytes(
        original.replace(
            b"name=recovery-check", b"name=" + label.encode("utf-8")
        )
    )
    assert machine()["records"][0]["name"] == label
    audit.write_bytes(
        original.replace(
            b"name=recovery-check", b"name=before\x1b[31mred\x1b[0mafter"
        )
    )
    assert machine()["records"][0]["name"] == "beforeredafter"
    for invalid_utf8 in (
        b"\x80",
        b"\xc0\xaf",
        b"\xe0\x80\xaf",
        b"\xf0\x80\x80\xaf",
        b"\xed\xa0\x80",
        b"\xf4\x90\x80\x80",
        b"\xe2\x82",
        b"\xf5\x80",
    ):
        audit.write_bytes(
            original.replace(b"name=recovery-check", b"name=" + invalid_utf8)
        )
        value = machine(succeeds=False)
        assert value["records"][0]["name"] is None, value
        assert value["status"] == "partial", value
        assert value["summary"]["serialization_issues"] == 1, value
    audit.write_bytes(
        original.replace(
            b"name=recovery-check", b'name=TOKEN="DO_NOT_LEAK_MACHINE token"'
        )
    )
    assert "DO_NOT_LEAK_MACHINE" not in json.dumps(machine())
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
        value = machine(succeeds=False)
        assert value["records"] == [], value
        assert value["summary"]["invalid_or_unreadable"] == 1, value
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
    text = run("recovery", "--limit", "129", "list", readonly=True)
    assert "129 recorded upgrade failures" in text, text
    absent = machine("--name", "absent", "--limit", "129")
    assert absent["records"] == [] and absent["complete"]
    assert absent["selection"] == {"examined": 129, "filtered_out": 129}
    limited = machine("--name", "absent", "--limit", "1", succeeds=False)
    assert limited["records"] == [] and limited["summary"]["limited"]
    assert limited["selection"] == {"examined": 1, "filtered_out": 1}
    assert "limited: no" in text, text
    text = run(
        "recovery", "list", "--limit", "1", succeeds=False, readonly=True
    )
    assert "1 recorded upgrade failures" in text, text
    for value in ("0", "4097", "99999999999999999", "1x", "+1", "-1", ""):
        run("recovery", "list", "--limit", value, succeeds=False, readonly=True)
    run(
        "recovery",
        "--limit",
        "1",
        "--limit",
        "2",
        succeeds=False,
        readonly=True,
    )
    run("recovery", "--limit", succeeds=False, readonly=True)
    value = machine("--limit", "129")
    names = [record["record"] for record in value["records"]]
    assert names == sorted(names, key=str.casefold, reverse=True), names
    assert value["summary"]["records"] == 129, value
    value = machine("--limit", "1", succeeds=False)
    assert value["summary"]["limited"] is True, value
    for copy in copies:
        copy.unlink()
    audit_dir = data / "audit"
    saved_dir = data / "saved-audit"
    audit_dir.rename(saved_dir)
    if junction(audit_dir, saved_dir):
        try:
            run("recovery", "list", succeeds=False, readonly=True)
            value = machine(succeeds=False)
            assert value["status"] == "unavailable", value
        finally:
            audit_dir.rmdir()
    else:
        print("SKIP: fixture filesystem cannot create a directory junction")
    saved_dir.rename(audit_dir)
    run("recovery", "list", readonly=True)
    # Quiescent whole-state restore into a fresh disposable explicit root.
    # External repository and signing paths retain their original locations.
    restored = root / "restored-data"
    before_data = snapshot(data)
    marker_before = marker.read_bytes(), marker.stat().st_mtime_ns
    shutil.copytree(data, restored, copy_function=shutil.copy2)
    assert snapshot(restored) == before_data
    env["WPM_DATA_DIR"] = str(restored)
    text = run("list", "recovery-check", readonly=True)
    assert "recovery-check any 1.0.0" in text, text
    assert "recovery-check any 1.0.1" not in text, text
    value = machine()
    assert value["records"][0]["record"] == audit.name, value
    assert value["records"][0]["new_version"] == "1.0.1", value
    run("repo", "verify", repository, readonly=True)
    run("repo", "diagnose", "--links", succeeds=False, readonly=True)
    assert snapshot(data) == before_data
    assert snapshot(restored) == before_data
    assert (marker.read_bytes(), marker.stat().st_mtime_ns) == marker_before
    env["WPM_DATA_DIR"] = str(data)

print("PASS: real failed upgrade audit query without mutation or script replay")
