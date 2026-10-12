"""Verify repository diagnosis is read-only, actionable and credential safe."""

import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time

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


with tempfile.TemporaryDirectory(prefix="wpm-repo-health-") as directory:
    root = Path(directory)
    data = root / "data"
    source = root / "repository"
    source.mkdir()
    source_index = source / "index.json"
    valid = json.dumps({"version": 1, "packages": []})
    source_index.write_text(valid)
    requests = []

    class Handler(SimpleHTTPRequestHandler):
        def do_GET(self):
            requests.append(self.path)
            super().do_GET()

        def log_message(self, *_arguments):
            pass

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(Handler, directory=str(source))
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    web = f"http://127.0.0.1:{server.server_port}"
    default = "https://github.com/Thewafflication/wpm/releases/latest/download"
    secret = "DO_NOT_LEAK_REPOSITORY_CREDENTIAL"
    credentialed = f"https://user:{secret}@example.invalid"
    env = dict(
        os.environ,
        WPM_DATA_DIR=str(data),
        WPM_HTTPS_BACKEND="invalid",
        WPM_LOG_FILE=str(root / "external.log"),
        WPM_LOG_DIR=str(root / "external-logs"),
    )

    def run(*arguments, succeeds=False):
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
        assert snapshot(root) == before
        assert not requests
        assert secret not in result.stdout + result.stderr
        assert not (root / "external.log").exists()
        assert not (root / "external-logs").exists()
        return result.stdout

    def cache(locator):
        value = 2166136261
        for byte in locator.encode():
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
        path = data / "cache" / "repositories" / f"{value:08x}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(valid)
        return path

    def machine(succeeds=False, links=False):
        options = ["--json"] + (["--links"] if links else [])
        text = run("repo", "diagnose", *options, succeeds=succeeds)
        report = json.loads(text)
        assert report["version"] == 1
        assert report["healthy"] is succeeds
        assert report["follow_links"] is links
        assert len(report["roots"]) == report["summary"]["inspected_roots"]
        assert [item["ordinal"] for item in report["roots"]] == list(
            range(1, len(report["roots"]) + 1)
        )
        for item in report["roots"]:
            assert set(item) == {
                "ordinal",
                "configured",
                "parent",
                "transport",
                "insecure",
                "status",
            }
            assert item["status"] in {
                "valid",
                "stale",
                "invalid",
                "unavailable",
            }
            assert item["insecure"] == (item["transport"] == "http")
        assert (
            str(source) not in text and web not in text and default not in text
        )
        return report

    try:
        text = run("repo", "diagnose")
        assert "unavailable: 1" in text and "wpm update" in text, text
        assert not data.exists()
        assert machine()["summary"]["unavailable"] == 1
        assert not data.exists()
        env["WPM_DATA_DIR"] = "C:\\" + "x" * 4085
        report = machine(links=True)
        assert report["error"] == "configuration_unreadable"
        assert report["summary"]["invalid_configuration"] == 1
        env["WPM_DATA_DIR"] = str(data)
        config = data / "config" / "repositories.txt"
        config.parent.mkdir(parents=True)
        entries = f"1000\t{source}\n1\t{web}\tallow-insecure-http=true\n"
        config.write_text(entries)
        web_cache = cache(web)
        cache(default)
        text = run("repo", "diagnose", succeeds=True)
        assert "usable indexes: 3" in text and "health healthy" in text, text
        assert "HTTP cache (insecure)" in text, text
        report = machine(succeeds=True)
        assert report["summary"]["usable_indexes"] == 3
        assert all(item["configured"] for item in report["roots"])
        json.loads(run("repo", "--json", "diagnose", succeeds=True))
        json.loads(
            run(
                "--color", "always", "repo", "diagnose", "--json", succeeds=True
            )
        )
        json.loads(
            run("repo", "diagnose", "--json", "--offline", succeeds=True)
        )
        run("--verbose", "repo", "diagnose", succeeds=True)
        run("repo", "diagnose", "--verbose", succeeds=True)
        run("repo", "diagnose", "--offline", succeeds=True)
        config.write_text(entries + f"0\t{credentialed}\n")
        text = run("--verbose", "repo", "diagnose")
        assert "invalid configuration: 1" in text, text
        assert machine()["summary"]["invalid_configuration"] == 1
        config.write_text(entries)

        stale = time.time() - 7200
        os.utime(web_cache, (stale, stale))
        text = run("repo", "diagnose")
        assert "stale: 1" in text and "Next action:" in text, text
        assert machine()["roots"][1]["status"] == "stale"
        now = time.time()
        os.utime(web_cache, (now, now))
        source_index.write_text('{"version":1,"packages":[],}')
        text = run("repo", "diagnose")
        assert "invalid: 1" in text and "check index/read access" in text, text
        assert machine()["summary"]["invalid"] == 1
        for number in (
            "1.0000000000000000000000001",
            "0.9999999999999999999999999",
        ):
            source_index.write_text('{"version":' + number + ',"packages":[]}')
            text = run("repo", "diagnose")
            assert "invalid: 1" in text, text
        source_index.write_text(valid)

        removed_source = root / "disconnected"
        source.rename(removed_source)
        try:
            text = run("repo", "diagnose")
            assert "unavailable: 1" in text and "Reconnect local media" in text
            assert machine()["roots"][0]["status"] == "unavailable"
        finally:
            removed_source.rename(source)
        config.write_text(entries + "2\tC:\\C:\n")
        text = run("repo", "diagnose")
        assert "invalid configuration: 1" in text, text
        config.write_text(entries + "not-a-repository-entry\n")
        text = run("repo", "diagnose")
        assert "invalid configuration: 1" in text, text
        config.write_text(entries + "# comment\n\n")
        run("repo", "diagnose", succeeds=True)
        config.write_text(entries)
        previous = source / "previous"
        nested = previous / "nested"
        nested.mkdir(parents=True)
        previous_index = previous / "index.json"
        previous_valid = json.dumps(
            {
                "version": 1,
                "packages": [],
                "repositories": ["nested"],
            }
        )
        previous_index.write_text(previous_valid)
        (nested / "index.json").write_text(valid)
        source_index.write_text(
            json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": ["previous", "previous"],
                }
            )
        )
        linked_web = web + "/previous"
        web_cache.write_text(
            json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": [linked_web],
                }
            )
        )
        linked_cache = cache(linked_web)
        linked_cache.write_text(
            json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": [web],
                }
            )
        )
        run("repo", "diagnose", succeeds=True)
        text = run("repo", "diagnose", "--links", succeeds=True)
        assert "Inspected roots: 6; link issues: 0" in text, text
        report = machine(succeeds=True, links=True)
        assert report["summary"]["inspected_roots"] == 6
        assert [item["parent"] for item in report["roots"]] == [
            0,
            0,
            0,
            1,
            2,
            4,
        ]
        assert sum(item["configured"] for item in report["roots"]) == 3
        assert "Linked from repository 2" in text, text
        linked_cache.unlink()
        text = run("repo", "diagnose", "--links")
        assert "unavailable: 1" in text, text
        linked_cache = cache(linked_web)
        os.utime(linked_cache, (stale, stale))
        text = run("repo", "diagnose", "--links")
        assert "stale: 1" in text, text
        now = time.time()
        os.utime(linked_cache, (now, now))
        previous_index.write_text('{"version":1,"packages":[],}')
        text = run("repo", "diagnose", "--links")
        assert "invalid: 1" in text, text
        previous_index.write_text(previous_valid)
        source_index.write_text(
            json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": ["previous", "../escape", credentialed],
                }
            )
        )
        text = run("repo", "diagnose", "--links")
        assert "link issues: 2" in text, text
        assert machine(links=True)["summary"]["link_issues"] == 2
        nodes = []
        for number in range(35):
            node = source / f"node-{number:02d}"
            node.mkdir()
            (node / "index.json").write_text(valid)
            nodes.append(node.name)
        source_index.write_text(
            json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": nodes,
                }
            )
        )
        web_cache.write_text(valid)
        text = run("repo", "diagnose", "--links")
        assert "Inspected roots: 32; link issues: 6" in text, text
        assert machine(links=True)["summary"]["link_issues"] == 6
        source_index.write_text(valid)
        full_config = "".join(f"1\t{source / node}\n" for node in nodes[:32])
        config.write_text(full_config + "# comment\n\n")
        report = machine(succeeds=True)
        assert report["summary"]["configured_roots"] == 32
        config.write_text(full_config + f"1\t{source / nodes[32]}\ninvalid\n")
        report = machine()
        assert report["summary"]["inspected_roots"] == 32
        assert report["summary"]["invalid_configuration"] == 2
        text = run("repo", "diagnose")
        assert "invalid configuration: 2" in text, text
        config.write_text(entries)
        for arguments in (
            ("repo", "diagnose", "extra"),
            ("repo", "diagnose", "--priority", "10"),
            ("repo", "diagnose", "--allow-insecure-http"),
            ("repo", "diagnose", "--unknown"),
            ("repo", "list", "--links"),
            ("repo", "add", source, "--links"),
            ("repo", "init", root / "invalid-json", "--json"),
            ("repo", "list", "--json"),
            ("repo", "diagnose", "--json", "--verbose"),
            ("--verbose", "repo", "diagnose", "--json"),
        ):
            text = run(*arguments)
            assert "Usage:" in text, text
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

print("PASS: repository health, remediation, read-only and secret-safe output")
