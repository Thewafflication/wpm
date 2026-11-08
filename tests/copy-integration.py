"""Exercise confirmed recursive copies and destination process shutdown."""

import argparse
import os
from pathlib import Path
import subprocess
import stat
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
parser.add_argument("--holder", required=True)
parser.add_argument("--legacy", action="store_true")
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())
holder_exe = str(Path(args.holder).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-copy-") as temporary:
    root = Path(temporary)
    source, destination = root / "source payload", root / "deployed app"
    (source / "nested" / "empty").mkdir(parents=True)
    (source / "nested" / "payload.dat").write_bytes(b"new nested data")
    (source / "top.txt").write_bytes(b"new top data")
    env = dict(os.environ, WPM_DATA_DIR=str(root / "unused-data"))

    def run(*arguments, answer="", ok=None):
        result = subprocess.run(
            [
                wpm,
                "copy",
                *(["--legacy"] if args.legacy else []),
                *map(str, arguments),
            ],
            env=env,
            input=answer,
            capture_output=True,
            text=True,
            timeout=60,
        )
        if ok is not None:
            assert (result.returncode == 0) == ok, (
                arguments,
                result.stdout,
                result.stderr,
            )
        return result

    run("--help", ok=True)
    help_result = subprocess.run(
        [wpm, "help", "copy"], capture_output=True, text=True, timeout=10
    )
    assert (
        help_result.returncode == 0 and "--recursive|-r" in help_result.stdout
    )
    for invalid in [
        (),
        (source,),
        (source, destination),
        ("--bogus",),
        (source, destination, "-r", "extra"),
        (source / "missing", destination, "-r"),
        (source / "*.txt", destination, "-r"),
    ]:
        run(*invalid, ok=False)
    for answer in ["", "\n", "n\n", "yes please\n"]:
        result = run(source, destination, "-r", answer=answer, ok=False)
        assert "[y/N]" in result.stdout
        assert not destination.exists()
    run(source, destination, "--recursive", "--dry-run", "--yes", ok=True)
    assert not destination.exists()
    run(source, destination, "--recursive", answer="y\n", ok=True)
    assert (
        destination / "nested" / "payload.dat"
    ).read_bytes() == b"new nested data"
    assert (destination / "nested" / "empty").is_dir()
    (destination / "keep.txt").write_bytes(b"destination only")
    run(source, destination, "-r", "-y", ok=True)
    assert (destination / "keep.txt").read_bytes() == b"destination only"

    # File copies use an exact filename, or its basename within an existing dir.
    run(source / "top.txt", root / "new" / "renamed.txt", "-y", ok=True)
    assert (root / "new" / "renamed.txt").read_bytes() == b"new top data"
    run(source / "top.txt", root / "new", "-y", ok=True)
    assert (root / "new" / "top.txt").read_bytes() == b"new top data"
    for name in ["CON", "NUL.txt", "COM1", "LPT1.log", "trailing.", "a:stream"]:
        run(source / "top.txt", root / name, "-y", ok=False)
    # A changed tree while the user considers confirmation requires a new plan.
    pending_target = root / "changed-plan-output"
    pending = subprocess.Popen(
        [wpm, "copy", str(source), str(pending_target), "-r"],
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        prompt = ""
        while not prompt.endswith("[y/N]: "):
            character = pending.stdout.read(1)
            assert character, prompt
            prompt += character
        addition = source / "added-while-prompting.txt"
        addition.write_bytes(b"changed tree")
        output, errors = pending.communicate("y\n", timeout=45)
        assert pending.returncode != 0, (output, errors)
        assert not pending_target.exists()
        addition.unlink()
    finally:
        if pending.poll() is None:
            pending.kill()
            pending.communicate(timeout=10)
    for target in [source, source / "nested", source / "newchild", root]:
        run(source, target, "-r", "-y", ok=False)
    # Reject a file/directory conflict during planning, before any copy.
    conflict = root / "conflict"
    conflict.mkdir()
    (conflict / "nested").write_bytes(b"not a directory")
    run(source, conflict, "-r", "-y", ok=False)
    assert not (conflict / "top.txt").exists()
    alias = root / "hardlink.txt"
    os.link(source / "top.txt", alias)
    run(source / "top.txt", alias, "-y", ok=False)
    # Junctions work without symbolic-link privilege on Windows.
    junction = root / "junction"
    made = subprocess.run(
        ["cmd", "/c", "mklink", "/J", str(junction), str(destination)],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert made.returncode == 0, made.stderr
    try:
        run(source, junction, "-r", "-y", ok=False)
        run(junction, root / "linked-output", "-r", "-y", ok=False)
        run(source, junction / "child", "-r", "-y", ok=False)
    finally:
        junction.rmdir()

    target = destination / "nested" / "payload.dat"
    target.write_bytes(b"old locked data")
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
        result = run(source, destination, "-r", answer="n\n", ok=False)
        assert f"{holder.pid}\t" in result.stdout, result.stdout
        assert holder.poll() is None
        run(source, destination, "-r", "--dry-run", "-y", ok=True)
        assert holder.poll() is None
        # Invalid plans cannot terminate a holder, even with --yes.
        run(source, source / "nested", "-r", "-y", ok=False)
        assert holder.poll() is None
        run(source, destination, "-r", answer="yes\n", ok=True)
        holder.wait(timeout=10)
        assert unrelated.poll() is None
        assert target.read_bytes() == b"new nested data"
    finally:
        for child in (holder, unrelated):
            if child.poll() is None:
                child.kill()
            child.communicate(timeout=10)
    # Protected ancestors must survive and prevent any replacement.
    target.write_bytes(b"ancestor-held data")
    with target.open("rb"):
        run(source, destination, "-r", "-y", ok=False)
    assert target.read_bytes() == b"ancestor-held data"
    readonly = destination / "top.txt"
    os.chmod(readonly, stat.S_IREAD)
    try:
        run(source / "top.txt", readonly, "-y", ok=False)
    finally:
        os.chmod(readonly, stat.S_IREAD | stat.S_IWRITE)
    assert not (root / "unused-data").exists()

print("Confirmed recursive copy integration passed")
