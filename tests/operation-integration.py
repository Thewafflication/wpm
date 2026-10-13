"""Exercise real lifecycle plans, cancellation, and changed-input rejection."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

from repository_test_helpers import junction
import re

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-operation-") as directory:
    root = Path(directory)
    data = root / "data"
    source = root / "source"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    installed = root / "installed"
    removed = root / "removed"
    (meta / "package.txt").write_text(
        "name=plan-test\nversion=1.0.0\narch=any\ndebug=false\n"
    )
    for phase, marker in (("install", installed), ("remove", removed)):
        (meta / f"{phase}.cmd").write_text(
            f'@echo off\necho {phase}>"{marker}"\nexit /b 0\n'
        )
    (source / "payload").write_text("validated payload")
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    env.pop("WPM_LOG_FILE", None)
    env.pop("WPM_LOG_DIR", None)

    def run(*arguments, input_text=""):
        return subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input=input_text,
            text=True,
            capture_output=True,
            timeout=30,
        )

    result = run("build", source, root)
    assert result.returncode == 0, result.stdout
    archive = root / "plan-test-any-1.0.0.zip"
    for answer in ("", "\n", "n\n", "maybe\n"):
        result = run(
            "install",
            archive,
            "--allow-unsigned",
            "--confirm",
            input_text=answer,
        )
        assert result.returncode == 0, result.stdout
        for text in ("Operation plan:", "Plan digest:", "cancelled"):
            assert text in result.stdout, result.stdout
        assert not installed.exists()
        assert not list((data / "packages").glob("*.zip"))
        assert not (data / "recovery").exists()

    # Change config after the prompt but before approval. The typed error must
    # reject the stale plan before staging or a package script can run.
    process = subprocess.Popen(
        [wpm, "install", str(archive), "--allow-unsigned", "--confirm"],
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = bytearray()
    while not output.endswith(b"[y/N]: "):
        byte = process.stdout.read(1)
        assert byte, output.decode(errors="replace")
        output.extend(byte)
    (data / "config" / "changed.txt").write_text("changed after preview")
    remainder, _ = process.communicate(b"y\n", timeout=20)
    output.extend(remainder)
    assert process.returncode != 0, output
    assert b"recompute and approve" in output, output
    assert not installed.exists()

    # Replace the approved input with byte-identical contents. A digest alone
    # cannot detect this file/media identity substitution after the preview.
    original = archive.read_bytes()
    process = subprocess.Popen(
        [wpm, "install", str(archive), "--allow-unsigned", "--confirm"],
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = bytearray()
    while not output.endswith(b"[y/N]: "):
        byte = process.stdout.read(1)
        assert byte, output.decode(errors="replace")
        output.extend(byte)
    held_original = root / "held-original.zip"
    archive.rename(held_original)
    archive.write_bytes(original)
    assert archive.read_bytes() == held_original.read_bytes()
    remainder, _ = process.communicate(b"y\n", timeout=20)
    output.extend(remainder)
    assert process.returncode != 0, output
    assert b"recompute and approve" in output, output
    assert not installed.exists()
    held_original.unlink()
    # A stable file identity still permits explicit approval and execution.
    result = run(
        "install", archive, "--allow-unsigned", "--confirm", input_text="y\n"
    )
    assert result.returncode == 0, result.stdout
    assert installed.exists()
    records = list((data / "recovery").glob("*.json"))
    assert len(records) == 1, records
    record = json.loads(records[0].read_text(encoding="utf-8"))
    assert record["schema"] == "wpm.recovery.v1"
    assert record["status"] == "completed"
    assert record["phase"] == "completed"
    assert record["externalStateMayHaveChanged"] is True
    assert record["scriptExitCode"] == 0
    assert record["retryEligible"] is False
    assert record["safeActions"] == ["inspect"]
    assert record["completedPhases"] == [
        "retain-input",
        "stage",
        "extract",
        "validate",
        "script",
        "retain-archive",
        "audit",
        "cleanup",
    ]
    retained = record["controlledArtifacts"][0]
    assert retained["role"] == "recovery-input"
    assert retained["state"] == "retained"
    assert retained["relativePath"] == f"{record['operationId']}/input.zip"
    assert Path(retained["path"]).read_bytes() == archive.read_bytes()
    assert (
        record["planDigest"]
        == re.search(r"Plan digest: ([0-9a-f]{64})", result.stdout)[1]
    )
    assert (
        record["input"]["blake2b256"]
        == hashlib.blake2b(archive.read_bytes(), digest_size=32).hexdigest()
    )
    assert record["package"] == {
        "name": "plan-test",
        "arch": "any",
        "oldVersion": None,
        "newVersion": "1.0.0",
    }
    assert not list((data / "recovery").glob("*.new"))
    assert run("remove", archive.stem).returncode == 0
    installed.unlink()
    removed.unlink()

    result = run("install", archive, "--allow-unsigned")
    assert result.returncode == 0, result.stdout
    assert installed.exists()
    assert "Proceed with" not in result.stdout
    assert "Result: plan-test any installed" in result.stdout
    for answer in ("", "\n", "no\n"):
        result = run(
            "remove", "plan-test-any-1.0.0", "--confirm", input_text=answer
        )
        assert result.returncode == 0, result.stdout
        assert "1.0.0 -> removed" in result.stdout
        assert "cancelled" in result.stdout
        assert not removed.exists()
    result = run("remove", "plan-test-any-1.0.0")
    assert result.returncode == 0, result.stdout
    assert removed.exists()
    assert "Proceed with" not in result.stdout
    assert not list((data / "packages").glob("*.zip"))

    # Normal 1.x installs retain their interactive unknown-key trust flow.
    private = root / "compat.private"
    public = root / "compat.public"
    assert run("keygen", private, public).returncode == 0
    assert run("build", source, root, "--sign", private).returncode == 0
    installed.unlink()
    result = run("install", archive, "--confirm", input_text="y\n")
    assert result.returncode != 0, result.stdout
    assert not installed.exists()
    assert not list((data / "trust").glob("*.pub"))
    result = run("install", archive, input_text="y\n")
    assert result.returncode == 0, result.stdout
    assert "Trust it? [y/N]" in result.stdout, result.stdout
    assert installed.exists()
    assert list((data / "trust").glob("*.pub"))

    # A script failure retains its exact exit and external-state uncertainty.
    installed.unlink()
    (meta / "install.cmd").write_text(
        f'@echo off\necho attempted>"{installed}"\nexit /b 37\n',
        encoding="utf-8",
    )
    assert run("build", source, root, "--sign", private).returncode == 0
    before = set((data / "recovery").glob("*.json"))
    result = run(
        "install", archive, "--allow-unsigned", "--confirm", input_text="y\n"
    )
    assert result.returncode != 0, result.stdout
    added = set((data / "recovery").glob("*.json")) - before
    assert len(added) == 1, (added, result.stdout, result.stderr)
    failure = json.loads(added.pop().read_text(encoding="utf-8"))
    assert failure["status"] == "failed"
    assert failure["failedPhase"] == "script"
    assert failure["scriptExitCode"] == 37
    assert failure["externalStateMayHaveChanged"] is True
    assert "script" not in failure["completedPhases"]
    assert failure["completedPhases"][-1] == "validate"
    assert installed.exists()

    # Initial record failure blocks extraction and external script effects.
    installed.unlink()
    recovery = data / "recovery"
    retained = data / "retained-recovery"
    recovery.rename(retained)
    recovery.write_text("unrecognized object", encoding="utf-8")
    result = run(
        "install", archive, "--allow-unsigned", "--confirm", input_text="y\n"
    )
    assert result.returncode != 0, result.stdout
    assert "recovery evidence could not be committed" in result.stdout
    assert not installed.exists()
    assert recovery.read_text(encoding="utf-8") == "unrecognized object"
    recovery.unlink()
    retained.rename(recovery)

    # A recovery junction is not a supported record destination and must not
    # redirect journal writes outside the selected controlled root.
    outside = root / "outside-recovery"
    outside.mkdir()
    recovery.rename(retained)
    assert junction(recovery, outside), "junction fixture unavailable"
    try:
        result = run(
            "install",
            archive,
            "--allow-unsigned",
            "--confirm",
            input_text="y\n",
        )
        assert result.returncode != 0, result.stdout
        assert "recovery evidence could not be committed" in result.stdout
        assert not list(outside.iterdir())
        assert not installed.exists()
    finally:
        recovery.rmdir()
        retained.rename(recovery)

    # A later update failure preserves the last atomic document. The script
    # makes the current record read-only; WPM must neither unlink nor truncate
    # it in an attempt to force completion.
    (meta / "install.cmd").write_text(
        f'@echo off\nattrib +r "{recovery}\\*.json"\nexit /b 0\n',
        encoding="utf-8",
    )
    assert run("build", source, root, "--sign", private).returncode == 0
    before = set(recovery.glob("*.json"))
    result = run(
        "install", archive, "--allow-unsigned", "--confirm", input_text="y\n"
    )
    assert result.returncode != 0, result.stdout
    assert "recovery evidence could not be committed" in result.stdout
    added = set(recovery.glob("*.json")) - before
    assert len(added) == 1, (added, result.stdout, result.stderr)
    incomplete = json.loads(added.pop().read_text(encoding="utf-8"))
    assert incomplete["status"] == "in-progress"
    assert incomplete["phase"] == "script"
    assert incomplete["externalStateMayHaveChanged"] is True
    assert incomplete["scriptExitCode"] is None
    assert not list(recovery.glob("*.new"))
    for path in recovery.glob("*.json"):
        path.chmod(0o666)

    # Abrupt termination must leave a parseable pre-process record. Terminate
    # only the disposable process tree started by this test.
    ready = root / "interruption-ready"
    (meta / "install.cmd").write_text(
        f'@echo off\necho ready>"{ready}"\n'
        "powershell -NoProfile -Command Start-Sleep -Seconds 30\nexit /b 0\n",
        encoding="utf-8",
    )
    assert run("build", source, root, "--sign", private).returncode == 0
    before = set(recovery.glob("*.json"))
    process = subprocess.Popen(
        [wpm, "install", str(archive), "--allow-unsigned", "--confirm"],
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    process.stdin.write(b"y\n")
    process.stdin.flush()
    deadline = time.monotonic() + 15
    try:
        while not ready.exists() and time.monotonic() < deadline:
            assert process.poll() is None
            time.sleep(0.05)
        assert ready.exists(), "script did not reach interruption boundary"
        added = set(recovery.glob("*.json")) - before
        assert len(added) == 1, added
        interrupted_path = added.pop()
        original = interrupted_path.read_bytes()
        interrupted = json.loads(original)
        assert interrupted["status"] == "in-progress"
        assert interrupted["phase"] == "script"
        assert interrupted["externalStateMayHaveChanged"] is True
        assert interrupted["scriptExitCode"] is None
        assert Path(interrupted["input"]["path"]).exists()
    finally:
        if process.poll() is None:
            killed = subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                capture_output=True,
                timeout=15,
            )
            assert killed.returncode == 0, killed.stdout
        process.communicate(timeout=15)
    assert interrupted_path.read_bytes() == original
    assert not list(recovery.glob("*.new"))

print(
    "PASS: complete plans, safe cancellation, --yes, and stale-state rejection"
)
