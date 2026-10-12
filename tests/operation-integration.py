"""Exercise real lifecycle plans, cancellation, and changed-input rejection."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

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

print(
    "PASS: complete plans, safe cancellation, --yes, and stale-state rejection"
)
