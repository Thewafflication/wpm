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

    try:
        text = run("repo", "diagnose")
        assert "unavailable: 1" in text and "wpm update" in text, text
        assert not data.exists()
        config = data / "config" / "repositories.txt"
        config.parent.mkdir(parents=True)
        entries = f"1000\t{source}\n1\t{web}\tallow-insecure-http=true\n"
        config.write_text(entries)
        web_cache = cache(web)
        cache(default)
        text = run("repo", "diagnose", succeeds=True)
        assert "usable indexes: 3" in text and "health healthy" in text, text
        assert "HTTP cache (insecure)" in text, text
        run("--verbose", "repo", "diagnose", succeeds=True)
        run("repo", "diagnose", "--verbose", succeeds=True)
        run("repo", "diagnose", "--offline", succeeds=True)
        config.write_text(entries + f"0\t{credentialed}\n")
        text = run("--verbose", "repo", "diagnose")
        assert "invalid configuration: 1" in text, text
        config.write_text(entries)

        stale = time.time() - 7200
        os.utime(web_cache, (stale, stale))
        text = run("repo", "diagnose")
        assert "stale: 1" in text and "Next action:" in text, text
        now = time.time()
        os.utime(web_cache, (now, now))
        source_index.write_text('{"version":1,"packages":[],}')
        text = run("repo", "diagnose")
        assert "invalid: 1" in text and "check index/read access" in text, text
        source_index.write_text(valid)

        removed_source = root / "disconnected"
        source.rename(removed_source)
        try:
            text = run("repo", "diagnose")
            assert "unavailable: 1" in text and "Reconnect local media" in text
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
        for arguments in (
            ("repo", "diagnose", "extra"),
            ("repo", "diagnose", "--priority", "10"),
            ("repo", "diagnose", "--allow-insecure-http"),
            ("repo", "diagnose", "--unknown"),
        ):
            text = run(*arguments)
            assert "Usage:" in text, text
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

print("PASS: repository health, remediation, read-only and secret-safe output")
