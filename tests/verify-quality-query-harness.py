"""Verify quality harness baselines and immutable first-failure evidence."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
harness = Path(__file__).with_name("quality-query-fuzz.py")


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


with tempfile.TemporaryDirectory(prefix="wpm-quality-harness-") as directory:
    root = Path(directory)

    def run(binary, output, cases=3, succeeds=True, extra=()):
        result = subprocess.run(
            [
                sys.executable,
                str(harness),
                "--wpm",
                str(Path(binary).resolve()),
                "--output",
                str(output),
                "--cases",
                str(cases),
                "--seed",
                "20037",
                *extra,
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )
        assert (result.returncode == 0) == succeeds, (
            result.stdout + result.stderr
        )

    passed = root / "passed"
    run(args.wpm, passed)
    report = json.loads((passed / "report.json").read_text(encoding="utf-8"))
    assert report["status"] == "pass" and report["complete"], report
    assert [case["family"] for case in report["cases"]] == [
        "audit",
        "catalog",
        "retained",
    ], report
    assert (
        report["binary_sha256"]
        == hashlib.sha256(Path(args.wpm).read_bytes()).hexdigest()
    )
    assert (
        report["harness_sha256"]
        == hashlib.sha256((passed / "harness.py").read_bytes()).hexdigest()
    )
    assert (
        report["corpus_sha256"]
        == hashlib.sha256(
            (passed / "quality-query-corpus.json").read_bytes()
        ).hexdigest()
    )
    runtime = root / "runtime-baselines"
    # Exercise a noncanonical output spelling as well as Windows TEMP aliases.
    run(args.wpm, runtime / ".." / runtime.name, cases=4)
    runtime_report = json.loads(
        (runtime / "report.json").read_text(encoding="utf-8")
    )
    assert [case["family"] for case in runtime_report["cases"]] == [
        "audit",
        "catalog",
        "retained",
        "runtime",
    ]
    for case in runtime_report["cases"]:
        assert case["input_copy_complete"] and case["input_copy_intact"]
        assert Path(case["working_directory"]).parent == runtime.resolve()
    runtime_case = runtime_report["cases"][-1]
    assert runtime_case["environment"]["WPM_TLS_CA_FILE"] is None
    # Python rejects WPM-only flags before loading any script. This wrong binary
    # gives a real failed subprocess without patching harness internals.
    failed = root / "failed"
    run(sys.executable, failed, succeeds=False)
    report = json.loads((failed / "report.json").read_text(encoding="utf-8"))
    assert report["status"] == "fail" and not report["complete"], report
    assert len(report["cases"]) == 1, report
    first = failed / "case-000000"
    assert (first / "data" / "audit").is_dir()
    assert (first / "input-manifest.json").is_file()
    assert (first / "stderr.bin").read_bytes()
    assert json.loads((first / "result.json").read_text())["status"] == "fail"
    preserved = Path(report["cases"][0]["input_copy"])
    assert report["cases"][0]["input_copy_complete"]
    assert report["cases"][0]["input_copy_intact"]
    original = next((first / "data" / "audit").glob("*.txt"))
    original_bytes = original.read_bytes()
    original.write_bytes(b"changed after the failure")
    assert (
        preserved / original.relative_to(first)
    ).read_bytes() == original_bytes
    before = snapshot(root)
    run(args.wpm, failed, succeeds=False)
    assert snapshot(root) == before, "existing failure evidence overwritten"
    invalid = root / "invalid-count"
    run(args.wpm, invalid, cases=0, succeeds=False)
    assert not invalid.exists(), "invalid shape created a run directory"
    limited = root / "duration-limit"
    run(
        args.wpm,
        limited,
        cases=10000,
        succeeds=False,
        extra=("--duration", "1"),
    )
    report = json.loads((limited / "report.json").read_text(encoding="utf-8"))
    assert report["status"] == "inconclusive" and not report["complete"]
    assert report["reason"] == "duration_limit", report

    spec = importlib.util.spec_from_file_location("quality_harness", harness)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    code, stdout, stderr, reason = module.bounded_process(
        [sys.executable, "-c", "import sys;sys.stdout.write('x'*1000000)"],
        None,
        10,
        limit=4096,
    )
    assert reason == "output_limit" and len(stdout) == 4096, (code, reason)
    code, stdout, stderr, reason = module.bounded_process(
        [sys.executable, "-c", "import time;time.sleep(5)"],
        None,
        0.1,
    )
    assert reason == "timeout", (code, reason)

print("PASS: quality baselines and immutable first-failure evidence")
