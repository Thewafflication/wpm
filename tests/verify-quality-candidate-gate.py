"""Exercise candidate acceptance and independent release-blocking partitions."""

import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from repository_test_helpers import junction

gate = Path(__file__).resolve().parents[1] / "tools/quality-candidate-gate.py"
spec = importlib.util.spec_from_file_location("quality_candidate_gate", gate)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
revision = "a" * 40
earlier = "b" * 40
wsp_revision = "c" * 40


def snapshot(root):
    return {
        str(path.relative_to(root)): (
            "directory"
            if path.is_dir()
            else hashlib.sha256(path.read_bytes()).hexdigest(),
            path.stat().st_mtime_ns,
        )
        for path in (root, *sorted(root.rglob("*")))
    }


with tempfile.TemporaryDirectory(prefix="wpm-quality-gate-") as directory:
    root = Path(directory)
    manifest_path = root / "manifest.json"

    def write(name, document):
        target = root / name
        target.write_text(json.dumps(document), encoding="utf-8")
        return dict(
            path=name, sha256=hashlib.sha256(target.read_bytes()).hexdigest()
        )

    binaries = {
        arch: str(i) * 64 for i, arch in enumerate(module.ARCHITECTURES)
    }
    reports = []
    documents = {}
    for arch in module.ARCHITECTURES:
        for kind, names in module.PROFILES.items():
            name = f"{arch}-{kind}.json"
            report = dict(
                schema="wpm.quality-profile.v1",
                source_revision=revision,
                source_state="clean",
                architecture=arch,
                profile=kind,
                execution="native",
                status="Pass",
                complete=True,
                binary_sha256=binaries[arch],
                environment="test-only Windows disposable fixture",
                started="2026-10-12T00:00:00+00:00",
                finished="2026-10-12T00:00:01+00:00",
                baseline=dict(
                    wsp=wsp_revision,
                    dependencies="test-only",
                    toolchain="test-only",
                ),
                environment_review=dict(
                    operator="test-only",
                    procedure="test-only",
                    approval="test-only",
                ),
                cases=[
                    dict(
                        name=case,
                        status="Pass",
                        command="test-only expected rejection",
                        exit_code=1,
                        expected_exit_code=1,
                    )
                    for case in sorted(names)
                ],
            )
            documents[name] = report
            reports.append(write(name, report))
    original = write(
        "original-failure.json", dict(status="Fail", source_revision=earlier)
    )
    baseline = dict(
        schema="wpm.quality-candidate.v1",
        source_revision=revision,
        status="Pass",
        complete=True,
        baseline=dict(
            wsp=wsp_revision,
            dependencies="test-only",
            toolchains={arch: "test-only" for arch in module.ARCHITECTURES},
        ),
        binaries=binaries,
        reports=reports,
        findings=[
            dict(
                id="synthetic-finding",
                owner="test-only",
                baseline=earlier,
                requirement="REQ-0020.006",
                review="test-only review",
                severity="high",
                release_effect="nonblocking",
                disposition="fixed",
                correction_revision=revision,
                original_failure=original,
                rerun=reports[0],
            )
        ],
    )

    def run(value, expected=None):
        manifest_path.write_text(json.dumps(value), encoding="utf-8")
        before = snapshot(root)
        result = subprocess.run(
            [
                sys.executable,
                str(gate),
                "--manifest",
                str(manifest_path),
                "--source-revision",
                revision,
                "--wsp-revision",
                wsp_revision,
            ],
            capture_output=True,
            timeout=15,
        )
        assert result.stderr == b"", result.stderr
        assert snapshot(root) == before, "gate rewrote source evidence"
        report = json.loads(result.stdout)
        assert report["schema"] == "wpm.quality-gate.v1", report
        if expected:
            assert result.returncode == 1 and report["status"] == "Blocked", (
                report
            )
            assert report["diagnostics"] == [expected], report
        else:
            assert result.returncode == 0 and report["status"] == "Pass", report
            assert report["profiles"] == 36, report

    run(baseline)
    for status in (
        "Fail",
        "Blocked",
        "Inconclusive",
        "Not run",
        "Not applicable",
    ):
        value = copy.deepcopy(baseline)
        value["status"] = status
        run(value, "non-pass-candidate")
    for field, replacement, expected in (
        ("source_revision", earlier, "candidate-source-mismatch"),
        ("complete", False, "incomplete-candidate"),
        ("findings", None, "invalid-findings"),
        ("baseline", {}, "candidate-wsp-mismatch"),
        ("reports", reports[:-1], "missing-required-profile"),
        ("reports", [*reports, reports[0]], "duplicate-profile"),
        ("binaries", {"x64": "0" * 64}, "binary-matrix-missing"),
    ):
        value = copy.deepcopy(baseline)
        value[field] = replacement
        run(value, expected)
    for field, replacement, expected in (
        ("source_revision", earlier, "profile-source-mismatch"),
        ("source_state", "dirty", "dirty-profile-source"),
        ("execution", "build-only", "non-native-profile"),
        ("execution", "emulated", "non-native-profile"),
        ("status", "Inconclusive", "non-pass-profile"),
        ("complete", False, "incomplete-profile"),
        ("binary_sha256", "f" * 64, "binary-mismatch"),
        ("baseline", {}, "incomplete-baseline"),
        (
            "baseline",
            dict(wsp=earlier, dependencies="test-only", toolchain="test-only"),
            "profile-baseline-mismatch",
        ),
        ("environment", "", "missing-environment"),
        ("finished", "2020-01-01T00:00:00+00:00", "reversed-timestamps"),
        ("cases", [], "missing-required-scenario"),
        ("cases", [dict(name="install", status="Fail")], "non-pass-case"),
    ):
        value = copy.deepcopy(baseline)
        changed = copy.deepcopy(documents[reports[0]["path"]])
        changed[field] = replacement
        value["reports"][0] = write("changed.json", changed)
        run(value, expected)
    for field, replacement, expected in (
        ("owner", "", "untriaged-finding"),
        ("release_effect", "unknown", "blocking-or-unknown-finding"),
        ("release_effect", "blocking", "blocking-or-unknown-finding"),
        ("disposition", "open", "unresolved-finding"),
        ("correction_revision", earlier, "stale-correction"),
        ("baseline", revision, "failure-baseline-mismatch"),
        ("rerun", original, "unallocated-corrective-pass"),
    ):
        value = copy.deepcopy(baseline)
        value["findings"][0][field] = replacement
        run(value, expected)
    for name in ("../outside.json", "C:/outside.json", "x\\outside.json"):
        value = copy.deepcopy(baseline)
        value["reports"][0]["path"] = name
        run(value, "unsafe-evidence-path")
    value = copy.deepcopy(baseline)
    value["reports"][0]["sha256"] = "f" * 64
    run(value, "evidence-digest-mismatch")
    hardlink = root / "linked.json"
    os.link(root / reports[0]["path"], hardlink)
    value = copy.deepcopy(baseline)
    value["reports"][0]["path"] = hardlink.name
    run(value, "evidence-hard-link")
    hardlink.unlink()
    target = root / "target"
    target.mkdir()
    linked_report = target / "report.json"
    linked_report.write_bytes((root / reports[0]["path"]).read_bytes())
    alias = root / "alias"
    if os.name == "nt":
        junction(alias, target)
    else:
        alias.symlink_to(target, target_is_directory=True)
    value = copy.deepcopy(baseline)
    value["reports"][0]["path"] = "alias/report.json"
    run(value, "evidence-reparse" if os.name == "nt" else "evidence-link")
    optical_index = next(
        i for i, item in enumerate(reports) if "optical" in item["path"]
    )
    value = copy.deepcopy(baseline)
    changed = copy.deepcopy(documents[reports[optical_index]["path"]])
    changed["environment_review"] = {}
    value["reports"][optical_index] = write("unreviewed.json", changed)
    run(value, "incomplete-environment-review")
    value = copy.deepcopy(baseline)
    rewritten = write(
        "rewritten-failure.json", dict(status="Pass", source_revision=earlier)
    )
    value["findings"][0]["original_failure"] = rewritten
    run(value, "missing-original-failure")
    for raw, expected in (
        (b'{"schema":1,"schema":2}', "duplicate-json-member"),
        (b'{"status":NaN}', "nonfinite-json-number"),
        (b'{"status":', "invalid-json-document"),
        (b"[]", "document-not-object"),
    ):
        try:
            module.document(raw)
        except module.InvalidEvidence as error:
            assert str(error) == expected
        else:
            raise AssertionError("malformed candidate document admitted")
    run(baseline)

print(
    "PASS: candidate native matrix, scenarios, integrity and blocking findings"
)
