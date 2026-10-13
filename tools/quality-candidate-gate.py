"""Aggregate retained 2.0 candidate evidence; never execute or rewrite it."""

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat

ARCHITECTURES = ("x86", "x64", "arm64")
PROFILES = {
    "endurance": {"install", "remove", "upgrade"},
    "archive": {"malformed", "truncated", "unsafe-path"},
    "metadata": {"malformed", "identity-conflict"},
    "index": {"malformed", "interrupted-publication", "prior-cache-preserved"},
    "signature": {"invalid", "unknown-key", "revoked-key"},
    "path": {"traversal", "reparse", "locked"},
    "self-upgrade": {
        "interrupted-handoff",
        "retained-inputs",
        "revalidated-completion",
    },
    "recovery": {
        "failed-script",
        "tampered-input",
        "safe-retry",
        "safe-cleanup",
        "restore",
    },
    "usb": {"read-only", "absent", "drive-letter-change", "interrupted-read"},
    "optical": {"read-only", "absent", "copied-tree"},
    "smb": {"authenticated-share", "disconnect", "partial-read"},
    "unavailable-repository": {"unavailable", "prior-cache-preserved"},
}
MANUAL = {"usb", "optical", "smb"}
MAX_BYTES = 4 * 1024 * 1024
MAX_TOTAL_BYTES = 64 * 1024 * 1024
evidence_bytes = 0
HEX256 = re.compile(r"[0-9a-f]{64}\Z")
REVISION = re.compile(r"(?:[0-9a-f]{40}|[0-9a-f]{64})\Z")


class InvalidEvidence(Exception):
    pass


def require(condition, code):
    if not condition:
        raise InvalidEvidence(code)


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, "duplicate-json-member")
        value[key] = item
    return value


def reject_constant(_):
    raise InvalidEvidence("nonfinite-json-number")


def document(raw):
    require(len(raw) <= MAX_BYTES, "document-size-limit")
    try:
        value = json.loads(
            raw.decode("utf-8"),
            object_pairs_hook=unique_object,
            parse_constant=reject_constant,
        )
    except (UnicodeError, ValueError, RecursionError) as error:
        raise InvalidEvidence("invalid-json-document") from error
    require(isinstance(value, dict), "document-not-object")
    return value


def plain_file(path):
    global evidence_bytes
    # Reject links/reparse points before resolving or reading any evidence.
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode), "evidence-not-regular-file")
    require(not info.st_nlink > 1, "evidence-hard-link")
    require(
        not getattr(info, "st_file_attributes", 0) & 0x400, "evidence-reparse"
    )
    require(info.st_size <= MAX_BYTES, "document-size-limit")
    evidence_bytes += info.st_size
    require(evidence_bytes <= MAX_TOTAL_BYTES, "evidence-size-limit")
    with path.open("rb") as stream:
        raw = stream.read(info.st_size + 1)
    require(len(raw) == info.st_size, "evidence-size-changed")
    return raw


def evidence(root, reference):
    require(isinstance(reference, dict), "invalid-evidence-reference")
    name, expected = reference.get("path"), reference.get("sha256")
    require(isinstance(name, str) and len(name) <= 512, "invalid-evidence-path")
    require(
        all(
            ord(char) >= 32
            and ord(char) != 127
            and not 0xD800 <= ord(char) <= 0xDFFF
            for char in name
        ),
        "unsafe-evidence-path",
    )
    require(
        isinstance(expected, str) and HEX256.fullmatch(expected), "bad-digest"
    )
    relative = PurePosixPath(name)
    require(
        not relative.is_absolute()
        and relative.parts
        and all(part not in ("..", ".") for part in relative.parts)
        and "\\" not in name
        and ":" not in name,
        "unsafe-evidence-path",
    )
    path = root
    for part in relative.parts:
        path = path / part
        info = path.lstat()
        require(not stat.S_ISLNK(info.st_mode), "evidence-link")
        require(
            not getattr(info, "st_file_attributes", 0) & 0x400,
            "evidence-reparse",
        )
    raw = plain_file(path)
    require(
        hashlib.sha256(raw).hexdigest() == expected, "evidence-digest-mismatch"
    )
    return document(raw)


def text(value):
    return isinstance(value, str) and 0 < len(value) <= 4096


def timestamp(value):
    require(text(value), "missing-timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise InvalidEvidence("invalid-timestamp") from error
    require(parsed.tzinfo is not None, "timestamp-without-offset")
    return parsed


def profile(root, reference, revision, binaries, expected_baseline):
    report = evidence(root, reference)
    require(report.get("schema") == "wpm.quality-profile.v1", "profile-schema")
    require(
        report.get("source_revision") == revision, "profile-source-mismatch"
    )
    require(report.get("source_state") == "clean", "dirty-profile-source")
    arch, kind = report.get("architecture"), report.get("profile")
    require(arch in ARCHITECTURES and kind in PROFILES, "unknown-profile")
    require(report.get("execution") == "native", "non-native-profile")
    require(report.get("status") == "Pass", "non-pass-profile")
    require(report.get("complete") is True, "incomplete-profile")
    require(report.get("binary_sha256") == binaries[arch], "binary-mismatch")
    require(text(report.get("environment")), "missing-environment")
    require(
        timestamp(report.get("finished")) >= timestamp(report.get("started")),
        "reversed-timestamps",
    )
    baseline = report.get("baseline")
    require(isinstance(baseline, dict), "missing-baseline")
    require(
        all(
            text(baseline.get(key))
            for key in ("wsp", "dependencies", "toolchain")
        ),
        "incomplete-baseline",
    )
    require(
        baseline["wsp"] == expected_baseline["wsp"]
        and baseline["dependencies"] == expected_baseline["dependencies"]
        and baseline["toolchain"] == expected_baseline["toolchains"][arch],
        "profile-baseline-mismatch",
    )
    if kind in MANUAL:
        review = report.get("environment_review")
        require(isinstance(review, dict), "missing-environment-review")
        require(
            all(
                text(review.get(key))
                for key in ("operator", "procedure", "approval")
            ),
            "incomplete-environment-review",
        )
    cases = report.get("cases")
    require(
        isinstance(cases, list) and len(cases) <= 1000, "invalid-profile-cases"
    )
    names = set()
    for case in cases:
        require(isinstance(case, dict), "invalid-case")
        name = case.get("name")
        require(text(name) and name not in names, "duplicate-or-invalid-case")
        names.add(name)
        require(case.get("status") == "Pass", "non-pass-case")
        require(
            type(case.get("exit_code")) is int
            and type(case.get("expected_exit_code")) is int
            and case["exit_code"] == case["expected_exit_code"],
            "unexpected-case-exit",
        )
        require(text(case.get("command")), "missing-case-command")
    require(PROFILES[kind] <= names, "missing-required-scenario")
    return arch, kind


def validate(manifest, revision, wsp_revision):
    global evidence_bytes
    evidence_bytes = 0
    require(REVISION.fullmatch(revision), "invalid-source-revision")
    require(REVISION.fullmatch(wsp_revision), "invalid-wsp-revision")
    root = manifest.parent.resolve(strict=True)
    value = document(plain_file(manifest))
    require(
        value.get("schema") == "wpm.quality-candidate.v1", "candidate-schema"
    )
    require(
        value.get("source_revision") == revision, "candidate-source-mismatch"
    )
    require(value.get("status") == "Pass", "non-pass-candidate")
    require(value.get("complete") is True, "incomplete-candidate")
    expected_baseline = value.get("baseline")
    require(isinstance(expected_baseline, dict), "missing-candidate-baseline")
    require(
        expected_baseline.get("wsp") == wsp_revision, "candidate-wsp-mismatch"
    )
    require(
        text(expected_baseline.get("dependencies")),
        "missing-dependency-baseline",
    )
    toolchains = expected_baseline.get("toolchains")
    require(
        isinstance(toolchains, dict)
        and set(toolchains) == set(ARCHITECTURES)
        and all(text(item) for item in toolchains.values()),
        "missing-toolchain-baseline",
    )
    binaries = value.get("binaries")
    require(isinstance(binaries, dict), "missing-binaries")
    require(set(binaries) == set(ARCHITECTURES), "binary-matrix-missing")
    require(
        all(
            isinstance(item, str) and HEX256.fullmatch(item)
            for item in binaries.values()
        ),
        "invalid-binary-digest",
    )
    reports = value.get("reports")
    require(
        isinstance(reports, list) and len(reports) <= 128, "invalid-report-list"
    )
    seen = set()
    for reference in reports:
        key = profile(root, reference, revision, binaries, expected_baseline)
        require(key not in seen, "duplicate-profile")
        seen.add(key)
    require(
        seen == {(arch, kind) for arch in ARCHITECTURES for kind in PROFILES},
        "missing-required-profile",
    )
    findings = value.get("findings")
    require(
        isinstance(findings, list) and len(findings) <= 1000, "invalid-findings"
    )
    ids = set()
    for finding in findings:
        require(isinstance(finding, dict), "invalid-finding")
        identifier = finding.get("id")
        require(text(identifier) and identifier not in ids, "duplicate-finding")
        ids.add(identifier)
        require(
            all(
                text(finding.get(key))
                for key in ("owner", "baseline", "requirement", "review")
            ),
            "untriaged-finding",
        )
        require(
            finding.get("severity") in ("low", "medium", "high", "critical"),
            "finding-severity",
        )
        require(
            finding.get("release_effect") == "nonblocking",
            "blocking-or-unknown-finding",
        )
        require(finding.get("disposition") == "fixed", "unresolved-finding")
        require(
            finding.get("correction_revision") == revision, "stale-correction"
        )
        original = evidence(root, finding.get("original_failure"))
        require(original.get("status") == "Fail", "missing-original-failure")
        require(
            original.get("source_revision") == finding["baseline"],
            "failure-baseline-mismatch",
        )
        require(finding.get("rerun") in reports, "unallocated-corrective-pass")
        rerun = evidence(root, finding.get("rerun"))
        require(
            rerun.get("source_revision") == revision
            and rerun.get("status") == "Pass",
            "missing-corrective-pass",
        )
    return len(seen)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--wsp-revision", required=True)
    args = parser.parse_args()
    try:
        count = validate(args.manifest, args.source_revision, args.wsp_revision)
        result = dict(
            status="Pass", complete=True, profiles=count, diagnostics=[]
        )
        code = 0
    except (InvalidEvidence, OSError, TypeError, KeyError, ValueError) as error:
        # Emit stable codes only; hostile paths/documents may contain secrets.
        reason = (
            str(error)
            if isinstance(error, InvalidEvidence)
            else "unreadable-or-invalid-evidence"
        )
        result = dict(status="Blocked", complete=False, diagnostics=[reason])
        code = 1
    print(
        json.dumps(
            dict(schema="wpm.quality-gate.v1", **result), separators=(",", ":")
        )
    )
    return code


if __name__ == "__main__":
    raise SystemExit(main())
