"""Seeded, isolated read-only query corpus with retained failure evidence."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import threading
import time
import zipfile


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            value.update(chunk)
    return value.hexdigest()


def snapshot(root):
    result = {}
    pending = [root]
    while pending:
        for path in sorted(pending.pop().iterdir()):
            information = path.lstat()
            assert not getattr(information, "st_file_attributes", 0) & 0x400, (
                "fixture contains a reparse point"
            )
            if path.is_dir():
                result[str(path.relative_to(root))] = (
                    "directory",
                    information.st_mtime_ns,
                )
                pending.append(path)
            else:
                assert information.st_size <= 2 * 1024 * 1024, (
                    "fixture file exceeds the snapshot bound"
                )
                result[str(path.relative_to(root))] = (
                    digest(path),
                    information.st_mtime_ns,
                )
    return result


def write_json(path, value):
    candidate = path.with_name(path.name + ".new")
    with candidate.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(value, indent=2) + "\n")
        stream.flush()
        os.fsync(stream.fileno())
    candidate.replace(path)


def bounded_process(argv, env, timeout, limit=256 * 1024):
    """Bound captured output and process lifetime, retaining the first bytes."""
    process = subprocess.Popen(
        argv,
        env=env,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    buffers = (bytearray(), bytearray())
    overflow = threading.Event()

    def read_stream(stream, buffer):
        while chunk := stream.read(4096):
            remaining = limit - len(buffer)
            buffer.extend(chunk[:remaining])
            if len(chunk) > remaining:
                overflow.set()
                try:
                    process.kill()
                except OSError:
                    pass
                break

    readers = [
        threading.Thread(target=read_stream, args=(stream, buffer), daemon=True)
        for stream, buffer in zip((process.stdout, process.stderr), buffers)
    ]
    for reader in readers:
        reader.start()
    reason = None
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        reason = "timeout"
        process.kill()
        process.wait(timeout=5)
    for reader in readers:
        reader.join(timeout=1)
    if overflow.is_set():
        reason = "output_limit"
    for stream, reader in zip((process.stdout, process.stderr), readers):
        if reader.is_alive():
            reason = "output_pipe_timeout"
        else:
            stream.close()
    return process.returncode, bytes(buffers[0]), bytes(buffers[1]), reason


def default_cache(data):
    locator = "https://github.com/Thewafflication/wpm/releases/latest/download"
    value = 2166136261
    for byte in locator.encode():
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    directory = data / "cache" / "repositories"
    directory.mkdir(parents=True)
    write_json(directory / f"{value:08x}.json", dict(version=1, packages=[]))
    # The precise Python clock can lead legacy GetSystemTimeAsFileTime.
    stamp = time.time() - 1
    os.utime(directory / f"{value:08x}.json", (stamp, stamp))


def audit_case(case, data, rng, baseline, secret):
    directory = data / "audit"
    directory.mkdir(parents=True)
    values = {
        "name": b"quality",
        "arch": b"any",
        "old-version": b"1.0.0",
        "new-version": b"1.0.1",
        "archive": b"quality-any-1.0.1.zip",
        "phase": b"install-script",
        "status": b"failed",
        "verification": b"failed",
        "exit-code": b"7",
    }
    changes = (
        b"",
        b"x" * 128,
        b"../outside",
        b"4294967296",
        b"-1",
        b"\x80",
        b"\xed\xa0\x80",
        b"\xf4\x90\x80\x80",
        b"\xe2\x82",
        "名💡".encode(),
        b"\x1b[31mquality\x1b[0m",
        b"TOKEN='" + secret.encode() + b" spaces'",
        b"https://user:" + secret.encode() + b"@example.invalid",
        b'quote"backslash\\tab\t',
        b"\0embedded",
    )
    if not baseline:
        values[rng.choice(list(values))] = rng.choice(changes)
    text = b"\n".join(
        key.encode() + b"=" + value for key, value in values.items()
    )
    if not baseline and rng.randrange(5) == 0:
        text += rng.choice((b"\nname=duplicate", b"\nunknown=extension", b"\0"))
    (directory / "20260101T000000.000Z-quality.upgrade-failed.txt").write_bytes(
        text
    )
    return ["recovery", "list"], "audit"


def catalog_case(case, data, rng, baseline, secret):
    source = case / "repository"
    source.mkdir()
    configuration = data / "config"
    configuration.mkdir(parents=True)
    (configuration / "repositories.txt").write_text(
        f"100\t{source}\n", encoding="utf-8"
    )
    default_cache(data)
    entry = dict(name="quality", version="1.0.0", arch="any", url="package.zip")
    catalog = dict(version=1, packages=[entry])
    if not baseline:
        mutation = rng.randrange(9)
        if mutation == 0:
            catalog["version"] = rng.choice((True, "1", 2, 0, None))
        elif mutation == 1:
            entry[rng.choice(list(entry))] = rng.choice(
                (None, 0, [], "", "../outside.zip", "unsupported")
            )
        elif mutation == 2:
            catalog["packages"] *= rng.choice((0, 2, 96, 97))
        elif mutation == 3:
            catalog["repositories"] = ["missing", "../outside"]
        elif mutation == 4:
            entry["name"] = rng.choice(
                ("名💡", "TOKEN='" + secret + " spaces'", 'quote"value')
            )
        elif mutation == 5:
            entry["version"] = rng.choice(
                ("1.0.0-alpha.1", "1.0.0+build.7", "01.0.0", "1", "1.0.-1")
            )
        elif mutation == 6:
            catalog["extension"] = {"nested": [None, True, {"value": "é"}]}
    text = json.dumps(catalog, ensure_ascii=False).encode()
    if not baseline and rng.randrange(5) == 0:
        text = rng.choice(
            (
                text[:-1],
                text + b" trailing",
                b'{"version":1,"version":1,"packages":[]}',
                text.replace(b"quality", b"bad-\x80"),
                b'{"version":1.000000000000000001,"packages":[]}',
            )
        )
    (source / "index.json").write_bytes(text)
    return ["list", "--available"], "catalog"


def installed_case(case, data, rng, baseline, secret):
    store = data / "packages"
    store.mkdir(parents=True)
    metadata = b"name=quality\nversion=1.0.0\narch=any\ndebug=false\n"
    if not baseline:
        metadata = rng.choice(
            (
                metadata,
                metadata.replace(b"quality", b"bad-\x80"),
                metadata.replace(b"1.0.0", b"1.0.0-alpha.1"),
                metadata.replace(b"1.0.0", b"01.0.0"),
                metadata.replace(b"any", b"unsupported"),
                metadata.replace(b"quality", b"TOKEN=" + secret.encode()),
                metadata + b"\0trailing",
                b"x" * (1024 * 1024 + 1),
            )
        )
    archive = store / "quality.zip"
    with zipfile.ZipFile(
        archive, "w", compression=zipfile.ZIP_DEFLATED
    ) as package:
        package.writestr(".wpm/package.txt", metadata)
        package.writestr(
            ".wpm/install.cmd",
            f'@echo off\necho replayed>"{case / "script-replayed"}"\n',
        )
        package.writestr("payload", "Read-only queries never extract this.")
    if not baseline and rng.randrange(7) == 0:
        archive.write_bytes(archive.read_bytes()[: rng.randrange(16)])
    return ["list"], "retained"


def verify_document(document, family, returncode):
    assert document["exit_code"] == returncode
    assert isinstance(document["complete"], bool)
    if family == "audit":
        assert document["schema"] == "wpm.recovery-list.v1"
        assert document["authenticated"] is False
        assert document["external_state_known"] is False
        assert document["automatic_retry_supported"] is False
        assert document["summary"]["records"] == len(document["records"])
    else:
        assert document["schema"] == "wpm.package-query.v1"
        assert document["metadata_only"] is True
        assert document["archive_integrity_verified"] is False
        assert document["signature_trust_verified"] is False
        assert document["deployed_state_known"] is False
        query = document[family]
        assert query["summary"]["matches"] == len(query["records"])
        assert query["complete"] == document["complete"]
    assert (returncode == 0) == document["complete"]
    assert document["status"] == ("complete" if returncode == 0 else "partial")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wpm", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument(
        "--seed", type=lambda value: int(value, 0), default=20037
    )
    parser.add_argument("--cases", type=int, default=120)
    parser.add_argument("--timeout", type=int, default=10)
    parser.add_argument("--source-revision")
    parser.add_argument(
        "--source-state",
        choices=("clean", "dirty", "unknown"),
        default="unknown",
    )
    parser.add_argument("--duration", type=int, default=120)
    parser.add_argument("--max-artifacts-mb", type=int, default=64)
    args = parser.parse_args()
    if not 3 <= args.cases <= 10000 or not 1 <= args.timeout <= 120:
        parser.error("cases must be 3..10000 and timeout 1..120 seconds")
    if not 1 <= args.duration <= 3600 or not 1 <= args.max_artifacts_mb <= 1024:
        parser.error("duration must be 1..3600 and artifacts 1..1024 MiB")
    wpm = Path(args.wpm).resolve(strict=True)
    if not wpm.is_file():
        parser.error("wpm must identify a binary file")
    corpus_path = Path(__file__).with_name("quality-query-corpus.json")
    corpus_bytes = corpus_path.read_bytes()
    corpus = json.loads(corpus_bytes.decode("utf-8"))
    if corpus.get("schema") != "wpm.quality-query-corpus.v1":
        parser.error("unsupported corpus metadata schema")
    output = Path(args.output).resolve()
    if output.exists():
        parser.error(
            "output must be a fresh directory; existing evidence is kept"
        )
    output.mkdir(parents=True)
    harness_bytes = Path(__file__).read_bytes()
    (output / "harness.py").write_bytes(harness_bytes)
    (output / "quality-query-corpus.json").write_bytes(corpus_bytes)
    write_json(output / "corpus-manifest.json", corpus)
    rng = random.Random(args.seed)
    report = dict(
        schema="wpm.quality-query-run.v1",
        started=datetime.now(timezone.utc).isoformat(),
        seed=args.seed,
        requested_cases=args.cases,
        binary=str(wpm),
        binary_sha256=digest(wpm),
        source_revision=args.source_revision,
        source_state=args.source_state,
        harness_sha256=hashlib.sha256(harness_bytes).hexdigest(),
        corpus_sha256=hashlib.sha256(corpus_bytes).hexdigest(),
        platform=platform.platform(),
        python=platform.python_version(),
        timeout_seconds=args.timeout,
        duration_seconds=args.duration,
        artifact_limit_bytes=args.max_artifacts_mb * 1024 * 1024,
        stream_limit_bytes=256 * 1024,
        cases=[],
    )
    generators = (audit_case, catalog_case, installed_case)
    failure = None
    bounded = False
    run_started = time.monotonic()
    artifact_bytes = 0
    report.update(complete=False, status="running")
    write_json(output / "report.json", report)
    for number in range(args.cases):
        remaining = args.duration - (time.monotonic() - run_started)
        retained_bytes = (
            artifact_bytes
            + (output / "report.json").stat().st_size
            + (output / "corpus-manifest.json").stat().st_size
            + (output / "harness.py").stat().st_size
            + (output / "quality-query-corpus.json").stat().st_size
        )
        if remaining <= 0 or retained_bytes >= report["artifact_limit_bytes"]:
            bounded = True
            report["reason"] = (
                "duration_limit" if remaining <= 0 else "artifact_limit"
            )
            break
        case = output / f"case-{number:06d}"
        case.mkdir()
        data = case / "data"
        secret = "QUALITY_SYNTHETIC_SECRET"
        command, family = generators[number % 3](
            case, data, rng, number < 3, secret
        )
        argv = [str(wpm), "--color", "always", *command, "--json", "--offline"]
        env = dict(
            os.environ,
            WPM_DATA_DIR=str(data),
            WPM_LOG_FILE=str(case / "external.log"),
            WPM_LOG_DIR=str(case / "external-logs"),
            WPM_HTTPS_BACKEND="invalid",
        )
        before = snapshot(case)
        write_json(case / "input-manifest.json", before)
        before = snapshot(case)
        started = time.monotonic()
        stdout, stderr, returncode = b"", b"", None
        outcome = dict(number=number, family=family, command=argv)
        try:
            budget = min(
                args.timeout, args.duration - (time.monotonic() - run_started)
            )
            if budget <= 0:
                bounded = True
                report["reason"] = "duration_limit"
                raise AssertionError("duration_limit")
            returncode, stdout, stderr, termination = bounded_process(
                argv, env, budget
            )
            if termination == "timeout" and budget < args.timeout:
                bounded = True
                report["reason"] = "duration_limit"
                raise AssertionError("duration_limit")
            assert termination is None, termination
            assert snapshot(case) == before, "read-only filesystem changed"
            assert returncode in (0, 1), (
                f"unexpected process status {returncode}"
            )
            assert stderr == b"", "stderr contaminated the machine query"
            assert stdout.endswith(b"\n"), "missing JSON terminator"
            assert b"\x1b" not in stdout, "terminal bytes in JSON"
            assert secret.encode() not in stdout + stderr, (
                "synthetic secret leaked"
            )
            verify_document(
                json.loads(stdout.decode("utf-8")), family, returncode
            )
            if number < 3:
                assert returncode == 0, "valid baseline query failed"
            outcome["status"] = "pass"
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b"", error.stderr or b""
            failure = "timeout"
        except (AssertionError, ValueError, KeyError, TypeError) as error:
            failure = str(error) or type(error).__name__
        except OSError as error:
            failure = (
                f"process/I/O error: {type(error).__name__} ({error.errno})"
            )
        (case / "stdout.bin").write_bytes(stdout)
        (case / "stderr.bin").write_bytes(stderr)
        outcome.update(
            returncode=returncode, elapsed_seconds=time.monotonic() - started
        )
        if failure:
            outcome.update(
                status="inconclusive" if bounded else "fail", reason=failure
            )
        write_json(case / "result.json", outcome)
        report["cases"].append(outcome)
        report.update(
            complete=not failure and number + 1 == args.cases,
            status="fail" if failure else "running",
        )
        write_json(output / "report.json", report)
        artifact_bytes += sum(
            path.stat().st_size for path in case.rglob("*") if path.is_file()
        )
        report["artifact_bytes"] = (
            artifact_bytes + (output / "report.json").stat().st_size
        )
        if failure:
            break
    report.update(
        status="inconclusive" if bounded else "fail" if failure else "pass",
        complete=not bounded and not failure,
        finished=datetime.now(timezone.utc).isoformat(),
    )
    write_json(output / "report.json", report)
    print(f"{report['status'].upper()}: {len(report['cases'])} cases; {output}")
    return 1 if failure or bounded else 0


if __name__ == "__main__":
    raise SystemExit(main())
