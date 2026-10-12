"""Verify local/cached linked catalog queries without writes or web requests."""

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
import zipfile

from repository_test_helpers import read_lock

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


def entry(name, version, arch="any", url="package.zip"):
    return dict(name=name, version=version, arch=arch, url=url)


with tempfile.TemporaryDirectory(prefix="wpm-available-list-") as directory:
    root = Path(directory)
    data = root / "data"
    source = root / "source"
    previous = source / "previous"
    previous.mkdir(parents=True)
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
    secret = "DO_NOT_LEAK_CATALOG_CREDENTIAL"
    credentialed = f"https://user:{secret}@example.invalid"
    env = dict(
        os.environ,
        WPM_DATA_DIR=str(data),
        WPM_HTTPS_BACKEND="invalid",
        WPM_LOG_FILE=str(root / "external.log"),
        WPM_LOG_DIR=str(root / "external-logs"),
    )

    def run(*arguments, succeeds=True, readonly=True):
        before = snapshot(root) if readonly else None
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="",
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        if readonly:
            assert snapshot(root) == before
        assert not requests
        assert secret not in result.stdout + result.stderr
        assert not (root / "external.log").exists()
        assert not (root / "external-logs").exists()
        return result.stdout

    def cache(locator, catalog):
        value = 2166136261
        for byte in locator.encode():
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
        path = data / "cache" / "repositories" / f"{value:08x}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(catalog))
        return path

    base = {
        "version": 1,
        "packages": [
            entry("alpha", "2.0.0"),
            entry("alpha", "10.0.0"),
            entry("alpha", "2.0.0", "x86"),
        ],
        "repositories": ["previous", "previous"],
    }
    empty = {"version": 1, "packages": []}
    source_index = source / "index.json"
    source_index.write_text(json.dumps(base))
    (previous / "index.json").write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [entry("alpha", "1.0.0")],
            }
        )
    )
    try:
        text = run("list", "--available", succeeds=False)
        assert "catalog issues: 1" in text, text
        assert not data.exists()
        text = run("show", "unknown", succeeds=False)
        assert "metadata is incomplete" in text, text
        for arguments in (
            ("show",),
            ("show", ""),
            ("show", "alpha", "extra"),
            ("show", "alpha", "--arch", "unknown"),
            ("show", "alpha", "--version", "invalid"),
            ("show", "alpha", "--available"),
            ("show", "alpha", "--yes"),
            ("show", "alpha", "--json"),
            ("show", "alpha", "--dry-run"),
            ("show", "alpha", "--repo", "33"),
            ("list", "--available", "--repo", "0"),
            ("list", "--repo", "1"),
        ):
            run(*arguments, succeeds=False)
        assert not data.exists()
        config = data / "config" / "repositories.txt"
        config.parent.mkdir(parents=True)
        configuration = f"200\t{source}\n100\t{web}\tallow-insecure-http=true\n"
        config.write_text(configuration)
        web_cache = cache(
            web,
            {
                "version": 1,
                "packages": [
                    entry("alpha", "3.0.0-beta.1"),
                    entry("beta", "1.0.0", "x64"),
                ],
                "repositories": [web + "/previous"],
            },
        )
        child_cache = cache(
            web + "/previous",
            {
                "version": 1,
                "packages": [entry("alpha", "2.0.0")],
                "repositories": [web],
            },
        )
        cache(default, empty)
        text = run("list", "--available")
        assert "7 matching catalog records" in text, text
        assert "inspected roots: 5" in text and "catalog issues: 0" in text
        assert (
            "prerelease excluded" in text and "Linked from repository" in text
        )
        assert web not in text and str(source) not in text
        rows = [
            line.strip()
            for line in text.splitlines()
            if line.startswith("  alpha any ")
        ]
        assert rows == [
            "alpha any " + version
            for version in ("10.0.0", "3.0.0-beta.1", "2.0.0", "2.0.0", "1.0.0")
        ], rows
        for ordinal, count in ((1, 3), (2, 2), (3, 0), (4, 1), (5, 1)):
            text = run("list", "--available", "--repo", ordinal)
            assert f"{count} matching catalog records" in text, text
            for line in text.splitlines():
                if "Repository:" in line:
                    assert f"Repository: {ordinal};" in line, text
        text = run("list", "--available", "--repo", "04", "alpha")
        assert "alpha any 1.0.0" in text, text
        text = run("list", "--available", "--repo", "32", succeeds=False)
        assert "ordinal 32 is absent" in text, text
        assert "0 matching catalog records" in text, text
        for value in ("0", "33", "99999999999999999", "1x", "+1", "-1", ""):
            run("list", "--available", "--repo", value, succeeds=False)
            run("show", "alpha", "--repo", value, succeeds=False)
        run("list", "--repo", "1", succeeds=False)
        run("list", "--available", "--repo", succeeds=False)
        run("show", "alpha", "--repo", "1", "--repo", "2", succeeds=False)
        store = data / "packages"
        store.mkdir()
        marker = root / "show-script-ran"
        with zipfile.ZipFile(store / "alpha-any-0.5.0.zip", "w") as package:
            package.writestr(
                ".wpm/package.txt",
                "name=alpha\nversion=0.5.0\narch=any\ndebug=false\n",
            )
            package.writestr(
                ".wpm/install.cmd",
                f'@echo off\necho ran>"{marker}"\nexit /b 0\n',
            )
        text = run("show", "ALPHA")
        assert "retained: 1; catalog: 6; complete: yes" in text, text
        assert "Multiple matching records" in text and "0.5.0" in text, text
        text = run("show", "alpha", "--repo", "4")
        assert "retained: 1; catalog: 1; complete: yes" in text, text
        assert "0.5.0" in text and "1.0.0" in text, text
        text = run("show", "alpha", "--repo", "32", succeeds=False)
        assert "retained: 1; catalog: 0; complete: no" in text, text
        text = run("show", "alpha", "--arch", "x86", "--offline")
        assert "retained: 0; catalog: 1; complete: yes" in text, text
        text = run("show", "alpha", "--version", "2.0.0")
        assert "retained: 0; catalog: 3; complete: yes" in text, text
        text = run("show", "unknown", succeeds=False)
        assert "no package metadata matches" in text, text
        run("--verbose", "show", "alpha")
        assert not marker.exists()
        for arguments, count in (
            (("list", "--available", "ALPHA"), 6),
            (("list", "alpha", "--available", "--arch", "x86"), 1),
            (("list", "--version", "2.0.0", "--available"), 3),
            (("list", "--available", "missing"), 0),
        ):
            text = run(*arguments)
            assert f"{count} matching catalog records" in text, text
        run("--verbose", "list", "--available", "--offline")
        (config.parent / "prerelease.txt").write_text("global=true\n")
        assert "prerelease allowed" in run("list", "--available", "alpha")
        stale = time.time() - 7200
        os.utime(web_cache, (stale, stale))
        text = run("list", "--available", succeeds=False)
        assert "stale cache" in text and "catalog issues: 1" in text
        # Python's precise clock may lead the legacy Windows API clock briefly.
        # Keep the fresh fixture in the past; future timestamps stay stale.
        now = time.time() - 1
        os.utime(web_cache, (now, now))
        future = time.time() + 120
        os.utime(web_cache, (future, future))
        text = run("list", "--available", succeeds=False)
        assert "stale cache" in text and "catalog issues: 1" in text, text
        os.utime(web_cache, (now, now))
        child_bytes = child_cache.read_bytes()
        child_cache.unlink()
        # A source filter never hides health problems elsewhere in the graph.
        text = run("list", "--available", "--repo", "1", succeeds=False)
        assert "3 matching catalog records" in text, text
        assert "catalog issues: 1" in text, text
        text = run("list", "--available", succeeds=False)
        assert (
            "6 matching catalog records" in text and "catalog issues: 1" in text
        ), text
        text = run("show", "alpha", succeeds=False)
        assert "retained: 1; catalog: 5; complete: no" in text, text
        text = run("show", "unknown", succeeds=False)
        assert "metadata is incomplete" in text, text
        child_cache.write_bytes(child_bytes)
        before = snapshot(root)
        with read_lock(source_index, share=0):
            run("list", "--available", succeeds=False, readonly=False)
        assert snapshot(root) == before
        for field, value in (
            ("version", "invalid"),
            ("arch", "unknown"),
            ("url", "../escape.zip"),
            ("name", 1),
        ):
            changed = json.loads(json.dumps(base))
            changed["packages"][0][field] = value
            source_index.write_text(json.dumps(changed))
            text = run("list", "--available", succeeds=False)
            assert "6 matching catalog records" in text, text
            assert "catalog issues: 1" in text, text
        source_index.write_text('{"version":1,"version":2,"packages":[]}')
        run("list", "--available", succeeds=False)
        changed = dict(
            base, repositories=["previous", "../escape", credentialed]
        )
        source_index.write_text(json.dumps(changed))
        text = run("list", "--available", succeeds=False)
        assert "catalog issues: 2" in text, text
        source_index.write_text(json.dumps(base))
        config.write_text(configuration + f"0\t{credentialed}\n")
        run("list", "--available", succeeds=False)
        config.write_text(configuration)
        nodes = []
        for number in range(35):
            node = source / f"node-{number:02d}"
            node.mkdir()
            (node / "index.json").write_text(json.dumps(empty))
            nodes.append(node.name)
        source_index.write_text(json.dumps(dict(base, repositories=nodes)))
        text = run("list", "--available", succeeds=False)
        assert "inspected roots: 32" in text and "catalog issues: 7" in text
        source_index.write_text(
            json.dumps(
                dict(
                    base,
                    packages=[
                        entry(f"many-{number:03d}", "1.0.0")
                        for number in range(100)
                    ],
                )
            )
        )
        text = run("list", "--available", succeeds=False)
        assert "96 matching catalog records" in text and "limited: yes" in text
        source_index.write_text(json.dumps(base))
        run("list", "--available")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

print(
    "PASS: linked local/cache catalog queries without writes or web retrieval"
)
