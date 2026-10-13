"""Verify planned retention preserves approved bytes after name replacement."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-admitted-input-") as directory:
    root = Path(directory)
    data = root / "data"
    original = root / "admitted-test-any-1.0.0.zip"
    replacement = root / "admitted-test-any-9.0.0.zip"
    moved = root / "approved-original.zip"
    marker = root / "source-replaced"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    env.pop("WPM_LOG_FILE", None)
    env.pop("WPM_LOG_DIR", None)

    def run(*arguments, input_text=""):
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input=input_text,
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        return result

    for version in ("1.0.0", "9.0.0"):
        source = root / f"source-{version}"
        metadata = source / ".wpm"
        metadata.mkdir(parents=True)
        (metadata / "package.txt").write_text(
            f"name=admitted-test\nversion={version}\narch=any\ndebug=false\n",
            encoding="utf-8",
        )
        (source / "payload").write_text(version, encoding="utf-8")
        if version == "1.0.0":
            (metadata / "install.cmd").write_text(
                "@echo off\n"
                f'move /y "{original}" "{moved}" >nul\n'
                "if errorlevel 1 exit /b 21\n"
                f'copy /y "{replacement}" "{original}" >nul\n'
                "if errorlevel 1 exit /b 22\n"
                f'echo replaced>"{marker}"\nexit /b 0\n',
                encoding="utf-8",
            )
        run("build", source, root)
    approved = original.read_bytes()
    changed = replacement.read_bytes()
    assert approved != changed
    run("install", original, "--allow-unsigned", "--confirm", input_text="y\n")
    assert marker.exists()
    assert original.read_bytes() == changed
    assert moved.read_bytes() == approved
    stored = data / "packages" / original.name
    assert stored.read_bytes() == approved, (
        "retention reopened the changed name"
    )
    records = list((data / "recovery").glob("*.json"))
    assert len(records) == 1
    record = json.loads(records[0].read_text(encoding="utf-8"))
    assert record["status"] == "completed"
    assert record["package"]["newVersion"] == "1.0.0"
    assert (
        record["input"]["blake2b256"]
        == hashlib.blake2b(approved, digest_size=32).hexdigest()
    )
    retained = data / "recovery" / record["operationId"] / "input.zip"
    assert retained.read_bytes() == approved

print("PASS: planned retention preserves approved bytes after name replacement")
