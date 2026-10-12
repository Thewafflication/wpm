"""Exercise linked graph refresh, cache reuse, cycles and failure fallback."""

import argparse
import ctypes
from ctypes import wintypes
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

        # A locked destination can reject publication even after a valid read.
        # Preserve the previous graph and prove a later retry can publish.
        publication_cache = cached(url)
        original_cache = (
            publication_cache.read_bytes(),
            publication_cache.stat().st_mtime_ns,
        )
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [
            wintypes.LPCWSTR,
            wintypes.DWORD,
            wintypes.DWORD,
            ctypes.c_void_p,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.HANDLE,
        ]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.CloseHandle.restype = wintypes.BOOL
        handle = kernel.CreateFileW(
            str(publication_cache), 0x80000000, 1, None, 3, 0, None
        )
        assert handle != wintypes.HANDLE(-1).value, ctypes.get_last_error()
        index(repository / "index.json", ["previous"], "replacement")
        requests.clear()
        try:
            text = run("update")
            assert "cache publication failed" in text, text
            assert "close programs locking" in text, text
            assert requests == expected, requests
            assert (
                publication_cache.read_bytes(),
                publication_cache.stat().st_mtime_ns,
            ) == original_cache
            assert not list((data / "cache").rglob("*.candidate*"))
        finally:
            kernel.CloseHandle(handle)
        requests.clear()
        run("update")
        assert requests == expected, requests
        assert (
            json.loads(publication_cache.read_text())["marker"] == "replacement"
        )

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
        # Valid JSON escapes in both keys and links must decode before lookup
        # and containment checks. Unknown Unicode fields remain skippable.
        escaped_indexes = (
            r'{"vers\u0069on":1,"packages":[],'
            r'"repositories":["\u0070revious"]}',
            r'{"\u00e9":0,"\u20ac":0,"\ud83d\ude00":0,"version":1,'
            r'"packages":[],"repositories":["previous"]}',
            r'{"quote\"slash\\tab\t":0,"version":1,'
            r'"packages":[],"repositories":'
            r'["previous","\u002e\u002e/escape"]}',
        )
        for contents in escaped_indexes:
            (repository / "index.json").write_text(contents)
            requests.clear()
            run("update")
            assert requests == expected, (contents, requests)
        # A retrieved catalog must pass schema admission before publication.
        # Rejection preserves exact cache bytes and mtime, then uses the known
        # web fallback graph without following the rejected catalog's links.
        root_cache = cached(url)
        before = (root_cache.read_bytes(), root_cache.stat().st_mtime_ns)
        (repository / "index.json").write_text(
            json.dumps(
                {
                    "version": 10,
                    "packages": [],
                    "repositories": ["rejected-root"],
                }
            )
        )
        requests.clear()
        text = run("update")
        assert requests == expected, requests
        assert "previous cache was retained" in text, text
        assert (
            root_cache.read_bytes(),
            root_cache.stat().st_mtime_ns,
        ) == before
        assert not list((data / "cache").rglob("*.candidate*"))
        for contents in (
            '{"version":1,"packages":[',
            '{"version":1,"packages":[]',
            '{"version":1,"packages":[],"note":"unterminated',
        ):
            (repository / "index.json").write_text(contents)
            requests.clear()
            text = run("update")
            assert requests == expected, requests
            assert "incomplete document" in text, text
            assert (
                root_cache.read_bytes(),
                root_cache.stat().st_mtime_ns,
            ) == before
            assert not list((data / "cache").rglob("*.candidate*"))
        # Grammar-invalid but balanced documents must retain the known cache.
        # The validator must also handle arbitrary legal extension values and
        # deep nesting without consuming the native call stack.
        malformed = (
            '{"version":1,"packages":[],}',
            '{"version":1 "packages":[]}',
            '{"version":1,"packages":[,]}',
            '{"version":1,"packages":[true,]}',
            '{"version":1,"packages":[]] }',
            '{"version":1,"packages":[],"extra":undefined}',
            '{"version":1,"packages":[],"extra":01}',
            '{"version":1,"packages":[],"extra":1.}',
            '{"version":1,"packages":[],"extra":1e+}',
            '{"version":1,"packages":[],"extra":tru}',
            '{"version":1,"packages":[],"extra":"bad\x01control"}',
            '{"version":1,"packages":[]}\x00hidden',
            '{"version":1,\x0b"packages":[]}',
            '{"version":1,"packages":[]} {}',
        )
        for contents in malformed:
            (repository / "index.json").write_bytes(contents.encode())
            requests.clear()
            text = run("update")
            assert requests == expected, (contents, requests)
            assert "previous cache was retained" in text, text
            assert (
                root_cache.read_bytes(),
                root_cache.stat().st_mtime_ns,
            ) == before
        valid_extension = {
            "long-extension-key-" * 10: [
                True,
                False,
                None,
                0,
                -2,
                1.25,
                1e50,
                "escaped\nvalue\x00",
            ],
            "version": 1,
            "packages": [],
            "repositories": ["previous"],
        }
        for contents in (
            json.dumps(valid_extension),
            '{"extension":'
            + "[" * 4096
            + "0"
            + "]" * 4096
            + ',"version":1,"packages":[],"repositories":["previous"]}',
        ):
            (repository / "index.json").write_text(contents)
            requests.clear()
            run("update")
            assert requests == expected, requests
        before = (root_cache.read_bytes(), root_cache.stat().st_mtime_ns)
        for key in (
            r"vers\qion",
            r"vers\u0000ion",
            r"vers\uD800ion",
            r"vers\uDC00ion",
            r"vers\uD800\u0041ion",
            r"vers\u12",
        ):
            contents = '{"' + key + '":1,"packages":[]}'
            (repository / "index.json").write_text(contents)
            root_cache.unlink(missing_ok=True)
            requests.clear()
            run("update", succeeds=False)
            assert requests == ["/index.json"], requests
            assert not root_cache.exists()
            assert not list((data / "cache").rglob("*.candidate*"))
        for invalid_index in (
            {"version": 10, "packages": [], "repositories": ["previous"]},
            {"metadata": {"version": 1}, "packages": []},
            {"version": "1", "packages": []},
            {"version": 1, "metadata": {"packages": []}},
        ):
            (repository / "index.json").write_text(json.dumps(invalid_index))
            root_cache.unlink(missing_ok=True)
            requests.clear()
            text = run("update", succeeds=False)
            assert requests == ["/index.json"], requests
            assert "unsupported schema" in text or "top-level" in text, text
            assert not root_cache.exists()
            assert not list((data / "cache").rglob("*.candidate*"))
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)

print("PASS: linked refresh, fresh/offline reuse, cycles, cache fallback")
