"""Verify HTTP length framing, 64-bit progress and cache retention."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
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

with tempfile.TemporaryDirectory(prefix="wpm-http-length-") as directory:
    root = Path(directory)
    data = root / "data"
    state = {"mode": "normal"}
    requests = []

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def do_GET(self):
            requests.append(self.path)
            mode = state["mode"]
            body = json.dumps(
                {
                    "version": 1,
                    "packages": [],
                    "marker": mode,
                }
            ).encode()
            self.send_response(200)
            self.send_header("Connection", "close")
            self.close_connection = True
            if mode == "chunked":
                self.send_header("Transfer-Encoding", "chunked")
            elif mode != "unknown":
                length = {
                    "short": str(len(body) + 100),
                    "large": str(2**32 + len(body)),
                    "invalid": "abc",
                    "overflow": str(2**64),
                    "conflicting": f"{len(body)},{len(body) + 1}",
                    "duplicate": f"{len(body)},{len(body)}",
                    "zero": "0",
                }.get(mode, str(len(body)))
                self.send_header("Content-Length", length)
            self.end_headers()
            if mode == "chunked":
                for offset in range(0, len(body), 17):
                    chunk = body[offset : offset + 17]
                    self.wfile.write(
                        f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n"
                    )
                self.wfile.write(b"0\r\n\r\n")
            elif mode != "zero":
                self.wfile.write(body)
            self.wfile.flush()

        def log_message(self, *_arguments):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f"http://127.0.0.1:{server.server_port}"
    env = dict(os.environ, WPM_DATA_DIR=str(data), WPM_HTTPS_BACKEND="invalid")
    for variable in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(variable, None)

    def run(*arguments):
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input="",
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert result.returncode == 0, result.stdout
        return result.stdout

    try:
        run("repo", "add", url, "--allow-insecure-http")
        value = 2166136261
        for byte in url.encode():
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
        cached = data / "cache" / "repositories" / f"{value:08x}.json"
        for mode in ("normal", "unknown", "chunked", "duplicate"):
            state["mode"] = mode
            run("update")
            assert json.loads(cached.read_text())["marker"] == mode
            assert not list((data / "cache").rglob("*.candidate*"))
        for mode in (
            "short",
            "large",
            "invalid",
            "overflow",
            "conflicting",
            "zero",
        ):
            state["mode"] = mode
            before = (cached.read_bytes(), cached.stat().st_mtime_ns)
            text = run("--verbose", "update")
            assert "using cached index" in text, text
            assert (cached.read_bytes(), cached.stat().st_mtime_ns) == before
            assert not list((data / "cache").rglob("*.candidate*"))
            if mode == "large":
                body = json.dumps(
                    {
                        "version": 1,
                        "packages": [],
                        "marker": mode,
                    }
                ).encode()
                assert (
                    f"HTTP response length: {2**32 + len(body)} bytes" in text
                )
            if mode in ("invalid", "overflow", "conflicting"):
                # WinInet may reject malformed framing before WPM receives it.
                assert "insecure HTTP read failed" in text, text
        state["mode"] = "normal"
        run("update")
        assert json.loads(cached.read_text())["marker"] == "normal"
        assert requests and all(path == "/index.json" for path in requests)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

print("PASS: HTTP lengths, unknown/chunked framing and cache-preserving faults")
