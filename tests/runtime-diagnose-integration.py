"""Read-only shared runtime diagnostics, UTF-8 JSON, and CLI boundaries."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
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
            path.stat().st_file_attributes,
        )
        for path in sorted(root.rglob("*"))
    }


with tempfile.TemporaryDirectory(prefix="wpm-runtime-query-") as directory:
    root = Path(directory)
    data = root / "absent-data"
    env = dict(os.environ, WPM_DATA_DIR=str(data), WPM_HTTPS_BACKEND="bundled")
    env["WPM_LOG_FILE"] = str(root / "absent-external.log")
    env["WPM_LOG_DIR"] = str(root / "absent-logs")
    env.pop("WPM_TLS_CA_FILE", None)

    def run(*arguments, binary=wpm):
        before = snapshot(root)
        result = subprocess.run(
            [binary, *map(str, arguments)],
            env=env,
            cwd=root,
            input=b"",
            capture_output=True,
            timeout=15,
        )
        assert snapshot(root) == before, arguments
        assert not result.stderr, result.stderr
        return result

    def machine(*arguments, binary=wpm, succeeds=True):
        result = run(*arguments, binary=binary)
        assert result.stdout.endswith(b"\n") and b"\x1b" not in result.stdout
        document = json.loads(result.stdout.decode("utf-8"))
        assert document["schema"] == "wpm.runtime-diagnose.v1"
        assert document["inspection"] == "runtime_configuration"
        assert isinstance(document["complete"], bool)
        assert document["complete"] == succeeds
        assert (
            result.returncode == document["exit_code"] == (0 if succeeds else 1)
        )
        assert document["status"] == ("complete" if succeeds else "partial")
        return document

    baseline = machine("--diagnose", "--json")
    assert baseline["executable"] == wpm
    assert baseline["data_root"] == str(data)
    assert baseline["package_directory"] == str(data) + "\\packages"
    assert baseline["cache_directory"] == str(data) + "\\cache"
    assert baseline["configuration_directory"] == str(data) + "\\config"
    assert baseline["https"] == {
        "backend": "bundled_mbedtls",
        "trust_source": "embedded_mozilla_bundle",
    }
    assert baseline["serialization_issues"] == 0
    for arguments in (
        ("--json", "--diagnose"),
        ("--diagnose", "--json", "--offline", "--dry-run"),
        ("--color", "always", "--diagnose", "--json"),
    ):
        assert machine(*arguments) == baseline
    human = run("--diagnose")
    assert human.returncode == 0
    for label, field in (
        ("Executable", "executable"),
        ("Data directory", "data_root"),
        ("Package directory", "package_directory"),
        ("Cache directory", "cache_directory"),
        ("Configuration directory", "configuration_directory"),
    ):
        assert (label + ": " + baseline[field]).encode() in human.stdout

    env["WPM_TLS_CA_FILE"] = str(root / "not-read-token=fixture-sensitive")
    configured = machine("--diagnose", "--json")
    assert configured["https"]["trust_source"] == "configured_ca_file"
    assert "fixture-sensitive" not in json.dumps(configured)
    env["WPM_HTTPS_BACKEND"] = "urlmon"
    invalid = machine("--diagnose", "--json", succeeds=False)
    assert invalid["https"] == {"backend": "invalid", "trust_source": None}
    assert run("--diagnose").returncode == 0  # Legacy exit semantics.
    env.pop("WPM_TLS_CA_FILE")
    assert machine("--diagnose", "--json")["https"] == {
        "backend": "urlmon",
        "trust_source": "windows_store",
    }
    env["WPM_HTTPS_BACKEND"] = "unsupported"
    assert (
        machine("--diagnose", "--json", succeeds=False)["https"]["backend"]
        == "invalid"
    )
    env["WPM_HTTPS_BACKEND"] = "bundled"
    env["WPM_DATA_DIR"] = "relative-state"
    relative = machine("--diagnose", "--json")
    assert relative["data_root"] == "relative-state"
    env["WPM_DATA_DIR"] = str(root / "token=fixture-sensitive")
    redacted = machine("--diagnose", "--json")
    assert "fixture-sensitive" not in json.dumps(redacted)
    assert redacted["data_root"].endswith("token=*")
    env["WPM_DATA_DIR"] = str(root / "caf\u00e9")
    native = run("--diagnose")
    native_root = native.stdout.split(b"Data directory: ", 1)[1].splitlines()[0]
    try:
        expected_root = native_root.decode("utf-8")
    except UnicodeDecodeError:
        encoding = machine("--diagnose", "--json", succeeds=False)
        assert encoding["data_root"] is None
        assert encoding["serialization_issues"] == 4
    else:
        assert machine("--diagnose", "--json")["data_root"] == expected_root
    env["WPM_DATA_DIR"] = str(root / ("x" * 280))
    unknown = machine("--diagnose", "--json", succeeds=False)
    assert unknown["data_root"] is None
    assert unknown["package_directory"] is None
    assert unknown["serialization_issues"] == 0
    env["WPM_DATA_DIR"] = str(data)
    for extra in (
        ("--verbose",),
        ("--json",),
        ("--diagnose",),
        ("--version",),
        ("install", "anything"),
        ("--allow-unsigned",),
        ("--offline", "--offline"),
    ):
        assert run("--diagnose", "--json", *extra).returncode == 1
    managed = root / "programs" / "WPM" / "wpm.exe"
    managed.parent.mkdir(parents=True)
    shutil.copy2(wpm, managed)
    env["ProgramFiles"] = env["ProgramW6432"] = str(managed.parent.parent)
    configured = machine("--diagnose", "--json", binary=str(managed))
    assert configured["runtime_mode"] == "managed"
    assert configured["executable"] == str(managed)
    assert (
        b"Runtime mode: managed"
        in run("--diagnose", binary=str(managed)).stdout
    )

print("PASS: shared runtime JSON, partial status, safe output, no writes")
