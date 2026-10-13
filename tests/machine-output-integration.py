"""Verify ADR-0010 envelopes, legacy query parity, and read-only errors."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

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
        for path in (root, *sorted(root.rglob("*")))
    }


with tempfile.TemporaryDirectory(prefix="wpm-envelope-") as directory:
    root = Path(directory)
    data = root / "data"
    secret = "DO_NOT_LEAK_ENVELOPE_SECRET"
    env = dict(
        os.environ,
        WPM_DATA_DIR=str(data),
        WPM_HTTPS_BACKEND="bundled",
        WPM_LOG_FILE=str(root / "external.log"),
        WPM_LOG_DIR=str(root / "external-logs"),
    )
    env.pop("WPM_TLS_CA_FILE", None)

    def run(*arguments, overrides=None):
        before = snapshot(root)
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            cwd=root,
            env=dict(env, **(overrides or {})),
            input=b"",
            capture_output=True,
            timeout=30,
        )
        assert snapshot(root) == before, "machine query mutated state"
        assert result.stderr == b"", result.stderr
        assert result.stdout.endswith(b"\n"), result.stdout
        assert b"\x1b" not in result.stdout, result.stdout
        assert secret.encode() not in result.stdout, result.stdout
        value = json.loads(result.stdout.decode("utf-8"))
        return result, value

    def envelope(*arguments, **kwargs):
        result, value = run(*arguments, "--output", "json", **kwargs)
        assert value["schema"] == "wpm.output.v1", value
        assert list(value) == [
            "schema",
            "command",
            "selectors",
            "resultSchema",
            "result",
            "diagnostics",
            "serializationComplete",
            "status",
            "exitCode",
        ], value
        assert value["exitCode"] == result.returncode
        if value["result"] is not None:
            assert value["resultSchema"]
            if "schema" in value["result"]:
                assert value["resultSchema"] == value["result"]["schema"]
        assert value["status"] in ("success", "partial", "error")
        assert (value["status"] == "success") == (result.returncode == 0)
        for event in value["diagnostics"]:
            assert set(event) == {
                "severity",
                "code",
                "operation",
                "phase",
                "message",
            }
        return result, value

    def parity(*arguments):
        old, legacy = run(*arguments, "--json")
        new, value = envelope(*arguments)
        assert value["result"] == legacy, value
        assert new.returncode == old.returncode, value
        assert value["serializationComplete"], value
        repeated, _ = envelope(*arguments)
        assert new.stdout == repeated.stdout, "nondeterministic envelope"
        return value

    for query in (
        ("--diagnose",),
        ("list",),
        ("list", "--available"),
        ("show", "absent"),
        ("repo", "diagnose"),
        ("repo", "diagnose", "--links"),
        ("recovery", "list"),
        ("recovery", "list", "--name", "absent", "--arch", "ANY"),
    ):
        parity(*query)
    assert not data.exists()

    catalog = root / "repository"
    catalog.mkdir()
    (catalog / "index.json").write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [
                    {
                        "name": "example",
                        "arch": "any",
                        "version": version,
                        "url": f"example-any-{version}.zip",
                    }
                    for version in ("1.0.0", "2.0.0", "2.1.0-beta.1")
                ],
            }
        )
    )
    config = data / "config"
    config.mkdir(parents=True)
    (config / "repositories.txt").write_text(f"1000|{catalog}\n")
    store = data / "packages"
    store.mkdir()
    for version in ("1.0.0", "2.0.0"):
        with zipfile.ZipFile(store / f"example-any-{version}.zip", "w") as z:
            z.writestr(
                ".wpm/package.txt",
                f"name=example\nversion={version}\narch=any\ndebug=false\n",
            )
            z.writestr(".wpm/install.cmd", f"echo ran>{root / 'ran'}\n")
    for query in (
        ("list", "example", "--arch", "ANY"),
        ("list", "--available", "example", "--version", "2.0.0"),
        ("list", "--available", "example", "--repo", "1"),
        ("show", "example", "--arch", "any", "--version", "1.0.0"),
        ("repo", "diagnose", "--links"),
    ):
        parity(*query)
    _, selected = envelope("list", "example", "--arch", "ANY")
    assert selected["selectors"]["arch"] == "any"
    assert selected["selectors"]["name"] == "example"
    assert selected["selectors"]["repository"] is None
    _, selected = envelope("list", "--available", "--repo", "1")
    assert selected["selectors"]["repository"] == 1
    assert selected["selectors"]["available"]
    _, selected = envelope("recovery", "--limit", "5")
    assert selected["selectors"]["limit"] == 5
    _, protected = envelope("list", f"token={secret}")
    assert protected["selectors"]["name"] == "token=*"
    parity("list", 'quoted"\\selector')

    for query in (
        (),
        ("show",),
        ("show", "example", "extra"),
        ("list", "--arch", "unsupported"),
        ("list", "--arch", f"token={secret}"),
        ("list", "--arch", "caf\u00e9"),
        ("list", "--arch", "any", "--arch", "x86"),
        ("list", "--version", "invalid"),
        ("list", "--repo", "0"),
        ("list", "--dry-run"),
        ("list", "--verbose"),
        ("list", "--json"),
        ("list", "--help"),
        ("recovery", "--limit", "4097"),
        ("recovery", "--name", ""),
        ("repo", "init", root / "should-not-exist"),
        ("install", "example", "--yes"),
        ("remove", "example", "--yes"),
        ("--complete-self-upgrade",),
        ("--diagnose", "list"),
        ("list", "--color", "invalid"),
        ("list", "--output", "json"),
        (f"token={secret}",),
    ):
        result, value = envelope(*query)
        assert result.returncode != 0, value
        assert value["result"] is None and value["diagnostics"], value
    for query in (
        ("list", "--output"),
        ("list", "--output", "text"),
        ("list", "--output=json", "--output=json"),
    ):
        result, value = run(*query)
        assert result.returncode and value["result"] is None, value

    _, baseline = envelope("list", "example")
    _, colored = envelope("--offline", "list", "example", "--color", "always")
    assert baseline == colored
    _, inline = run("--output=json", "list", "example")
    assert inline == baseline
    result, value = envelope("list", *("--verbose",) * 40)
    assert result.returncode and not value["serializationComplete"]
    assert len(value["diagnostics"]) == 32
    result, value = envelope(
        "--diagnose", overrides={"WPM_HTTPS_BACKEND": "invalid"}
    )
    assert result.returncode and value["result"]["status"] == "partial"
    assert value["diagnostics"][-1]["code"] == "output.query.incomplete"
    assert not (root / "ran").exists()
    assert not (root / "external.log").exists()

print("PASS: machine envelopes, query parity, structured errors, no mutations")
