"""Exercise shared redaction and a genuine hidden Windows console."""

import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
parser.add_argument("--child", action="store_true")
parser.add_argument("--root")
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())


def run(*arguments, env, capture=True):
    result = subprocess.run(
        [wpm, *arguments],
        env=env,
        capture_output=capture,
        text=True,
        timeout=30,
    )
    assert result.returncode == 0, (arguments, result.stdout, result.stderr)
    return result.stdout


def console_child(root):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetStdHandle.restype = wintypes.HANDLE
    handle = kernel.GetStdHandle(-11)
    mode = wintypes.DWORD()
    kernel.GetConsoleMode.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.DWORD),
    ]
    assert kernel.GetConsoleMode(handle, ctypes.byref(mode)), (
        "No native console"
    )

    class Coord(ctypes.Structure):
        _fields_ = [("x", wintypes.SHORT), ("y", wintypes.SHORT)]

    kernel.ReadConsoleOutputAttribute.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.WORD),
        wintypes.DWORD,
        Coord,
        ctypes.POINTER(wintypes.DWORD),
    ]
    attributes = (wintypes.WORD * 4096)()
    count = wintypes.DWORD()
    result = subprocess.run(
        [wpm, "--color", "auto", "unknown-command"], timeout=10
    )
    assert result.returncode != 0
    assert kernel.ReadConsoleOutputAttribute(
        handle, attributes, 4096, Coord(0, 0), ctypes.byref(count)
    )
    assert 12 in attributes[: count.value], "Auto error styling missing"
    env = os.environ.copy()
    env["WPM_DATA_DIR"] = str(root / "data")
    env["WPM_LOG_FILE"] = str(root / "operational.log")
    run(
        "upgrade",
        "--all",
        "--yes",
        "--offline",
        "--allow-unsigned",
        env=env,
        capture=False,
    )
    # Model the shell's prompt after the original process returns.
    print("WPM-TEST-PROMPT>", end="", flush=True)
    log = next((root / "data" / "audit").glob("self-upgrade-*-2.0.0.log"), None)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if log and "upgraded" in log.read_text(errors="replace"):
            break
        logs = list((root / "data" / "audit").glob("self-upgrade-*-2.0.0.log"))
        log = logs[0] if logs else None
        time.sleep(0.1)
    else:
        raise AssertionError("Background upgrade never completed")
    time.sleep(0.2)

    output = ctypes.create_unicode_buffer(32768)
    count = wintypes.DWORD()
    kernel.ReadConsoleOutputCharacterW.argtypes = [
        wintypes.HANDLE,
        wintypes.LPWSTR,
        wintypes.DWORD,
        Coord,
        ctypes.POINTER(wintypes.DWORD),
    ]
    assert kernel.ReadConsoleOutputCharacterW(
        handle, output, 32767, Coord(0, 0), ctypes.byref(count)
    )
    screen = output[: count.value].rstrip()
    assert screen.endswith("WPM-TEST-PROMPT>"), screen
    assert "Completion runs in the background" in screen, screen
    assert "Result: wpm" in screen and "scheduled" in screen, screen
    assert "upgraded" not in screen, screen
    assert "console-install-marker" in log.read_text(), log.read_text()
    (root / "console-evidence.json").write_text(
        json.dumps(
            {
                "nativeConsole": True,
                "promptPreserved": True,
                "completionLogged": True,
                "screen": screen,
            }
        )
    )


if args.child:
    console_child(Path(args.root))
    sys.exit(0)

with tempfile.TemporaryDirectory(prefix="wpm-presentation-") as directory:
    root = Path(directory)
    env = os.environ.copy()
    env["WPM_DATA_DIR"] = str(root / "data")
    env["WPM_LOG_FILE"] = str(root / "operational.log")
    # Public diagnostic fields must not expose URI credentials, protected
    # assignments, or caller-controlled terminal escapes, even before logging.
    for value, secret in (
        ("https://private-user:private-pass@example.invalid/", "private-pass"),
        ("token=private-token", "private-token"),
        ("secret-key=private-key", "private-key"),
        ("bad\x1b[31mcommand", "\x1b"),
        ("bad\x1b]0;injected-title\a", "injected-title"),
    ):
        result = subprocess.run(
            [wpm, value], env=env, capture_output=True, text=True, timeout=10
        )
        assert result.returncode != 0
        assert secret not in result.stdout, result.stdout
        assert secret not in result.stderr, result.stderr
    for selector in (("--arch", "mips"), ("--version", "invalid")):
        clean = root / "selector-data"
        selector_env = {**env, "WPM_DATA_DIR": str(clean)}
        result = subprocess.run(
            [wpm, "install", "missing", *selector],
            env=selector_env,
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert result.returncode != 0 and "Usage:" in result.stdout
        assert not clean.exists(), "Invalid selector created durable state"
    run("repo", "list", env=env)
    log = (root / "operational.log").read_text()
    assert "event=repository.info operation=repository phase=command" in log, (
        log
    )
    assert "\x1b" not in log, log
    binary = Path(wpm).read_bytes()
    offset = int.from_bytes(binary[0x3C:0x40], "little")
    machine = int.from_bytes(binary[offset + 4 : offset + 6], "little")
    arch = {0x14C: "x86", 0x8664: "x64", 0xAA64: "arm64"}[machine]
    repo = root / "repo"
    repo.mkdir()
    archives = []
    for version in ("1.0.0", "2.0.0"):
        source = root / version
        meta = source / ".wpm"
        meta.mkdir(parents=True)
        (meta / "package.txt").write_text(
            f"name=wpm\nversion={version}\narch={arch}\ndebug=false\n"
        )
        (meta / "install.cmd").write_text(
            "@echo off\necho console-install-marker\nexit /b 0\n"
        )
        shutil.copy2(wpm, source / "wpm.exe")
        runtime = Path(wpm).with_name("wcrt.dll")
        if runtime.exists():
            shutil.copy2(runtime, source / runtime.name)
        run("build", str(source), str(repo), env=env)
        archives.append(repo / f"wpm-{arch}-{version}.zip")
    store = root / "data" / "packages"
    store.mkdir(exist_ok=True)
    shutil.copy2(archives[0], store / archives[0].name)
    (repo / "index.json").write_text(
        json.dumps(
            {
                "version": 1,
                "packages": [
                    {
                        "name": "wpm",
                        "version": "2.0.0",
                        "arch": arch,
                        "url": archives[1].name,
                    }
                ],
            }
        )
    )
    run("repo", "add", str(repo), env=env)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    child = subprocess.run(
        [
            sys.executable,
            __file__,
            "--wpm",
            wpm,
            "--child",
            "--root",
            str(root),
        ],
        startupinfo=startup,
        creationflags=subprocess.CREATE_NEW_CONSOLE,
        timeout=60,
        env=env,
    )
    assert child.returncode == 0, "Native console regression failed"
    evidence = json.loads((root / "console-evidence.json").read_text())
    assert evidence["promptPreserved"] and evidence["completionLogged"]
    print(
        "PASS: shared redaction, typed log events, "
        "native console self-upgrade prompt"
    )
