"""Exercise discovery and opt-in termination of a disposable file holder."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
parser.add_argument("--legacy", action="store_true")
parser.add_argument("--holder", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())
holder_exe = str(Path(args.holder).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-file-users-") as directory:
    root = Path(directory)
    target = root / "upgrade destination.dat"
    target.write_bytes(b"old version")
    env = dict(os.environ, WPM_DATA_DIR=str(root / "unused-data"))

    def run(*arguments):
        return subprocess.run(
            [
                wpm,
                "file-processes",
                *(["--legacy"] if args.legacy else []),
                *map(str, arguments),
            ],
            env=env,
            capture_output=True,
            text=True,
            timeout=45,
        )

    assert run("--help").returncode == 0
    for invalid in [(), ("--kill",), ("--bogus",), (root,), (root / "*.dat",)]:
        result = run(*invalid)
        assert result.returncode != 0, (invalid, result.stdout, result.stderr)
    assert run(root / "new destination.dat").returncode == 0
    assert run(target).returncode == 0

    holder = subprocess.Popen(
        [holder_exe, "hold", str(target)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    unrelated = subprocess.Popen(
        [holder_exe, "sleep", "unused"], stdout=subprocess.DEVNULL
    )
    try:
        assert holder.stdout.readline().strip() == "ready"
        result = run(target, target)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert result.stdout.count(f"{holder.pid}\t") == 1, result.stdout
        assert holder.poll() is None
        result = run(target, "--kill", "--dry-run")
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert f"{holder.pid}\t" in result.stdout
        assert holder.poll() is None
        result = run(target, root, "--kill")
        assert result.returncode != 0
        assert holder.poll() is None
        result = run(target, "--kill")
        assert result.returncode == 0, (result.stdout, result.stderr)
        holder.wait(timeout=10)
        assert unrelated.poll() is None
        target.write_bytes(b"new version")
        result = run(target)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert f"{holder.pid}\t" not in result.stdout
        assert not (root / "unused-data").exists()
        # The test runner is an ancestor of WPM and must survive kill mode.
        with target.open("rb"):
            result = run(target, "--kill")
            assert f"{os.getpid()}\t" in result.stdout, result.stdout
            assert result.returncode != 0, (result.stdout, result.stderr)
    finally:
        for child in (holder, unrelated):
            if child.poll() is None:
                child.kill()
            child.communicate(timeout=10)

    # A DLL mapped into a same-architecture process has no persistent open file
    # handle. This exercises the module scan as well as the handle scan above.
    dll = root / "private-version.dll"
    module_holder = subprocess.Popen(
        [holder_exe, "module", str(dll)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        assert module_holder.stdout.readline().strip() == "ready"
        result = run(dll)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert f"{module_holder.pid}\t" in result.stdout, result.stdout
        assert module_holder.poll() is None
        result = run(dll, "--kill")
        assert result.returncode == 0, (result.stdout, result.stderr)
        module_holder.wait(timeout=10)
        dll.write_bytes(b"replaced DLL")
    finally:
        if module_holder.poll() is None:
            module_holder.kill()
        module_holder.communicate(timeout=10)

print("File-process discovery and termination checks passed.")
