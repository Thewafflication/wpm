"""Exercise linked graph refresh, cache reuse, cycles and failure fallback."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-linked-refresh-") as directory:
    root = Path(directory)
    data = root / "data"
    repository = root / "repository"
    previous = repository / "previous"
    grandchild = previous / "grandchild"
    grandchild.mkdir(parents=True)
    requests = []
    unavailable = set()

    class Handler(SimpleHTTPRequestHandler):
        def do_GET(self):
            requests.append(self.path)
            if self.path in unavailable:
                self.send_error(503)
            else:
                super().do_GET()

        def log_message(self, *_arguments):
            pass

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(Handler, directory=str(repository))
    )
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    url = f"http://127.0.0.1:{server.server_port}"
    env = dict(os.environ, WPM_DATA_DIR=str(data), WPM_HTTPS_BACKEND="invalid")
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

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

    def index(path, links, marker):
        path.write_text(
            json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": links,
                    "marker": marker,
                }
            )
        )

    def cached(locator):
        value = 2166136261
        for byte in locator.encode():
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
        return data / "cache" / "repositories" / f"{value:08x}.json"

    try:
        # The descendant links back to its parent; duplicates and cycles must
        # be deduplicated before fetching, not merely stopped after retrieval.
        index(repository / "index.json", ["previous", "previous"], "root")
        index(previous / "index.json", ["grandchild", url], "old")
        index(grandchild / "index.json", [url], "nested-old")
        run("repo", "add", url, "--allow-insecure-http")
        run("update")
        expected = [
            "/index.json",
            "/previous/index.json",
            "/previous/grandchild/index.json",
        ]
        assert requests == expected, requests
        child_cache = cached(url + "/previous")
        nested_cache = cached(url + "/previous/grandchild")
        assert json.loads(child_cache.read_text())["marker"] == "old"

        index(previous / "index.json", ["grandchild", url], "new")
        index(grandchild / "index.json", [url], "nested-new")
        requests.clear()
        # Normal resolution still reuses fresh catalogs; no implicit refresh.
        run("install", "absent-package", succeeds=False)
        assert not requests, requests
        assert json.loads(child_cache.read_text())["marker"] == "old"
        run("update", "--offline")
        assert not requests, requests
        run("install", "absent-package", "--dry-run", succeeds=False)
        assert not requests, requests

        # Explicit update refreshes all linked roots even inside freshness TTL.
        run("repo", "update")
        assert requests == expected, requests
        assert json.loads(child_cache.read_text())["marker"] == "new"
        assert json.loads(nested_cache.read_text())["marker"] == "nested-new"

        # A failed child refresh preserves its previous readable catalog and
        # still allows the graph's usable descendants to be visited.
        before = child_cache.read_bytes()
        unavailable.add("/previous/index.json")
        requests.clear()
        text = run("update")
        assert requests == expected, requests
        assert "using cached index" in text, text
        assert child_cache.read_bytes() == before
        assert not list((data / "cache").rglob("*.download"))
        # A hostile/unbounded link list must stop at the repository budget.
        unavailable.clear()
        links = [url + "/", "http://127.0.0.1:1/escape", "../escape"]
        for number in range(36):
            node = repository / f"node-{number:02d}"
            node.mkdir()
            index(node / "index.json", [], "bounded")
            links.append(node.name)
        index(repository / "index.json", links, "bounded-root")
        requests.clear()
        text = run("update")
        # Two configured roots (including the built-in HTTPS root) leave thirty
        # slots for linked roots. The root's self-link consumes no extra slot.
        assert requests == ["/index.json"] + [
            f"/node-{number:02d}/index.json" for number in range(30)
        ], requests
        assert "repository link capacity (32)" in text, text
        assert "unsafe repository link" in text, text
        # Schema recognition reads the root field, accepts JSON whitespace and
        # a UTF-8 BOM, and cannot be authorized by nested/string metadata.
        valid_indexes = (
            '{"version":\n  1, "packages": [], "repositories":["previous"]}',
            json.dumps(
                {
                    "metadata": {"version": 10, "note": 'quote " and }'},
                    "packages": [],
                    "version": 1,
                    "repositories": ["previous"],
                },
                indent=2,
            ),
            "\ufeff"
            + json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "repositories": ["previous"],
                }
            ),
            '{"version":1.0,"packages":[],"repositories":["previous"]}',
        )
        for contents in valid_indexes:
            (repository / "index.json").write_text(contents, encoding="utf-8")
            requests.clear()
            run("update")
            assert requests == expected, (contents, requests)
        for invalid_index in (
            {"version": 10, "packages": [], "repositories": ["previous"]},
            {"metadata": {"version": 1}, "packages": []},
            {"version": "1", "packages": []},
            {"version": 1, "metadata": {"packages": []}},
        ):
            (repository / "index.json").write_text(json.dumps(invalid_index))
            requests.clear()
            text = run("update", succeeds=False)
            assert requests == ["/index.json"], requests
            assert "unsupported schema" in text or "top-level" in text, text
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)

print("PASS: linked refresh, fresh/offline reuse, cycles, cache fallback")
