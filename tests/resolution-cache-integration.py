"""Resolution reuses aged caches and visits WPM history only on request."""

import argparse
from functools import partial
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
wpm = str(Path(parser.parse_args().wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-resolution-cache-") as directory:
    root = Path(directory)
    data, repository = root / "data", root / "repository"
    previous = repository / "previous"
    previous.mkdir(parents=True)
    requests = []

    class Handler(SimpleHTTPRequestHandler):
        def do_GET(self):
            requests.append(self.path)
            super().do_GET()

        def log_message(self, *_arguments):
            pass

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(Handler, directory=str(repository))
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f"http://127.0.0.1:{server.server_port}"
    env = dict(os.environ, WPM_DATA_DIR=str(data), WPM_HTTPS_BACKEND="invalid")
    for key in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(key, None)

    def run(*arguments, succeeds=True):
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="",
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        return result.stdout

    def cache(locator):
        value = 2166136261
        for byte in locator.encode():
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
        return data / "cache" / "repositories" / f"{value:08x}.json"

    def package(name, version, destination):
        source = root / f"{name}-{version}"
        metadata = source / ".wpm"
        metadata.mkdir(parents=True)
        (metadata / "package.txt").write_text(
            f"name={name}\nversion={version}\narch=any\n"
        )
        for phase in ("install", "remove", "upgrade"):
            (metadata / f"{phase}.cmd").write_text("@echo off\nexit /b 0\n")
        (source / "payload.txt").write_text(f"{name}-{version}")
        run("build", source, destination, "--sign", root / "private")
        return dict(
            name=name,
            version=version,
            arch="any",
            url=f"{name}-any-{version}.zip",
        )

    def index(path, packages, links):
        path.write_text(
            json.dumps(dict(version=1, packages=packages, repositories=links))
        )

    try:
        run("keygen", root / "private", root / "public")
        run("trust", "add", root / "public")
        current = package("wpm", "1.0.0", repository)
        old = package("wpm", "0.9.0", previous)
        index(repository / "index.json", [current], ["previous"])
        index(previous / "index.json", [old], [])
        default = cache(
            "https://github.com/Thewafflication/wpm/releases/latest/download"
        )
        default.parent.mkdir(parents=True, exist_ok=True)
        index(default, [], [])
        run("repo", "add", url, "--allow-insecure-http", "--priority", "1000")

        # First use retrieves the current index and selected archive only.
        run("install", "wpm", "--yes")
        assert requests == ["/index.json", "/wpm-any-1.0.0.zip"], requests
        assert not cache(url + "/previous").exists()

        # Age the cache beyond the former one-hour automatic refresh window.
        aged = time.time() - 7200
        for path in (cache(url), default):
            os.utime(path, (aged, aged))
        before = (cache(url).read_bytes(), cache(url).stat().st_mtime_ns)
        requests.clear()
        run("install", "absent-package", succeeds=False)
        run("upgrade", "wpm", "--yes")
        run("upgrade", "--all", "--yes")
        assert not requests, requests
        assert (
            cache(url).read_bytes(),
            cache(url).stat().st_mtime_ns,
        ) == before

        # An exact WPM selector admits the previous-release graph on demand.
        run("install", "wpm", "--version", "0.9.0", "--yes")
        assert requests == [
            "/previous/index.json",
            "/previous/wpm-any-0.9.0.zip",
        ], requests

        # Explicit update refreshes current WPM metadata, not its history.
        requests.clear()
        run("update")
        assert requests == ["/index.json"], requests
        assert cache(url).stat().st_mtime_ns != before[1]

        # General-purpose linked package roots still resolve dependencies.
        current_tool = package("tool", "1.0.0", repository)
        linked_tool = package("linked-tool", "1.0.0", previous)
        index(repository / "index.json", [current_tool], ["previous"])
        index(previous / "index.json", [linked_tool], [])
        run("update")
        for path in (cache(url), cache(url + "/previous")):
            os.utime(path, (aged, aged))
        requests.clear()
        run("install", "linked-tool", "--yes")
        assert requests == ["/previous/linked-tool-any-1.0.0.zip"], requests
        requests.clear()
        run("upgrade", "linked-tool", "--yes")
        assert not requests, requests

        # Live local resolution does not copy indexes back into the cache.
        local = root / "local"
        local.mkdir()
        local_tool = package("local-tool", "1.0.0", local)
        index(local / "index.json", [local_tool], [])
        run("repo", "add", local, "--priority", "2000")

        def cache_snapshot():
            return {
                p.name: (p.read_bytes(), p.stat().st_mtime_ns)
                for p in (data / "cache" / "repositories").glob("*.json")
            }

        before = cache_snapshot()
        requests.clear()
        run("install", "local-tool", "--yes")
        run("upgrade", "local-tool", "--yes")
        assert cache_snapshot() == before
        assert not requests, requests
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

print("PASS: aged cache reuse, explicit refresh, and on-demand WPM history")
