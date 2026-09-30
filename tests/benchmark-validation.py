"""Generate indexed files and compare two builds of WPM's actual verifier.

Requires Python 3.9+ and the opt-in wpm-validation-benchmark CMake target.
All generated data stays in a new work directory; nothing is deleted.
"""
import argparse
import csv
import hashlib
import json
import platform
from pathlib import Path
import statistics
import subprocess
import ctypes
from datetime import datetime, timezone


CASES = [("1000x1KiB", 1000, 1024), ("100x100KiB", 100, 100 * 1024),
         ("10x10MiB", 10, 10 * 1024**2), ("1x1GiB", 1, 1024**3)]


def generate(root, count, size):
    root.mkdir(parents=True)
    (root / ".wpm").mkdir()
    # Signature presence enables the verifier's completeness scan. This is a
    # fixture marker, NOT a signed package; no signature authentication is timed.
    (root / ".wpm/signature.json").write_text("{}\n", encoding="ascii")
    rows = ["filename,size,hash,algorithm\n"]
    for i in range(count):
        # Stable, file-specific, compressible 64 KiB pattern, no PRNG dependency.
        block = bytes((j * 37 + j // 256 + i) % 256 for j in range(65536))
        name = f"file-{i:04d}.bin"
        digest = hashlib.blake2b(digest_size=32)
        remaining = size
        with (root / name).open("wb") as output:
            while remaining:
                data = block[:min(remaining, len(block))]
                output.write(data)
                digest.update(data)
                remaining -= len(data)
        rows.append(f"{name},{size},{digest.hexdigest()},blake2b\n")
    (root / ".wpm/index.csv").write_text("".join(rows), encoding="ascii")


def run(executable, dataset, expected_valid=True, buffered=False, timeout=3600):
    command = [str(executable), str(dataset)] + (["--buffered"] if buffered else [])
    class IOCounters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_ulonglong) for name in
                    ("read_ops", "write_ops", "other_ops", "read_bytes", "write_bytes", "other_bytes")]
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.GetProcessIoCounters.argtypes = [ctypes.c_void_p, ctypes.POINTER(IOCounters)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
        handle = kernel.OpenProcess(0x1000, False, process.pid)
        counters = IOCounters()
        try:
            stdout, stderr = process.communicate(timeout=timeout)
            if not handle or not kernel.GetProcessIoCounters(handle, ctypes.byref(counters)):
                raise ctypes.WinError(ctypes.get_last_error())
        except BaseException:
            process.kill()
            process.communicate()
            raise
        finally:
            if handle:
                kernel.CloseHandle(handle)
    records = [line[6:] for line in stdout.splitlines() if line.startswith("BENCH ")]
    if len(records) != 1:
        raise RuntimeError(f"Missing benchmark record: {stdout}\n{stderr}")
    record = json.loads(records[0])
    if bool(record["valid"]) != expected_valid or (process.returncode == 0) != expected_valid:
        raise RuntimeError(f"Unexpected validation result: {stdout}\n{stderr}")
    record.update(process_read_ops=counters.read_ops, process_read_bytes=counters.read_bytes)
    return record


def negative_controls(executables, root, buffered=False):
    dataset = root / "negative-control"
    generate(dataset, 1, 1024)
    payload = dataset / "file-0000.bin"
    original = payload.read_bytes()
    checks = []
    for mode, executable in executables.items():
        run(executable, dataset, buffered=buffered)
        try:
            payload.write_bytes(bytes([original[0] ^ 1]) + original[1:])
            run(executable, dataset, False, buffered)
            payload.write_bytes(original[:-1])
            run(executable, dataset, False, buffered)
            payload.write_bytes(original)
            extra = dataset / "unindexed.bin"
            extra.write_bytes(b"unindexed")
            run(executable, dataset, False, buffered)
        finally:
            payload.write_bytes(original)
            # This file is created above solely for this negative control.
            extra = dataset / "unindexed.bin"
            if extra.exists():
                extra.unlink()
        checks.append({"mode": mode, "valid": "pass", "corruption": "rejected",
                       "truncation": "rejected", "unindexed_file": "rejected"})
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scalar", type=Path, required=True)
    parser.add_argument("--simd", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--cases", nargs="+", choices=[case[0] for case in CASES])
    parser.add_argument("--buffered", action="store_true", help="Experimental 64 KiB stdio buffering")
    parser.add_argument("--timeout", type=int, default=3600, help="Per-process timeout in seconds")
    args = parser.parse_args()
    if args.rounds < 1 or args.warmups < 0 or args.timeout < 1:
        parser.error("Use positive rounds/timeout and nonnegative warmups")
    cases = [case for case in CASES if not args.cases or case[0] in args.cases]
    executables = {"scalar": args.scalar.resolve(strict=True), "simd": args.simd.resolve(strict=True)}
    root = args.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    metadata = {
        "utc_started": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(), "python": platform.python_version(),
        "rounds": args.rounds, "warmups_per_backend_per_case": args.warmups,
        "cases": [case[0] for case in cases], "buffered": args.buffered,
        "units": "binary: KiB=1024, MiB=1048576, GiB=1073741824",
        "cache_policy": "regular OS-cached file I/O; no cache eviction; see warmup count",
        "executables": {k: {"path": str(v), "sha256": hashlib.sha256(v.read_bytes()).hexdigest()}
                        for k, v in executables.items()},
        "negative_controls": negative_controls(executables, root, args.buffered),
    }
    (root / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    rows = []
    for case, count, size in cases:
        dataset = root / case
        print(f"Generating {case} ({count * size:,} bytes)", flush=True)
        generate(dataset, count, size)
        for warmup in range(args.warmups):
            for mode in ("scalar", "simd") if warmup % 2 == 0 else ("simd", "scalar"):
                record = run(executables[mode], dataset, buffered=args.buffered, timeout=args.timeout)
                if record["bytes"] != count * size or record["progress_adds"] != count:
                    raise RuntimeError(f"Incorrect warmup coverage: {record}")
        for iteration in range(args.rounds):
            order = ("scalar", "simd") if iteration % 2 == 0 else ("simd", "scalar")
            for position, mode in enumerate(order):
                record = run(executables[mode], dataset, buffered=args.buffered, timeout=args.timeout)
                if record["bytes"] != count * size or record["progress_adds"] != count:
                    raise RuntimeError(f"Incorrect measured coverage: {record}")
                if mode == "scalar" and record["backend"] != "scalar":
                    raise RuntimeError("Scalar executable selected SIMD")
                if mode == "simd" and record["backend"] == "scalar":
                    raise RuntimeError("SIMD executable selected scalar; no SIMD comparison possible")
                row = {"case": case, "files": count, "bytes_per_file": size,
                       "round": iteration + 1, "position": position + 1, "mode": mode, **record}
                rows.append(row)
                print(f"  {iteration + 1:2d} {mode:6s}: {record['elapsed_ms']:.3f} ms", flush=True)
                with (root / "samples.csv").open("w", newline="", encoding="utf-8") as output:
                    writer = csv.DictWriter(output, fieldnames=rows[0].keys())
                    writer.writeheader()
                    writer.writerows(rows)
    summary = []
    for case, count, size in cases:
        item = {"case": case, "total_bytes": count * size}
        for mode in executables:
            samples = [r for r in rows if r["case"] == case and r["mode"] == mode]
            times = [r["elapsed_ms"] for r in samples]
            median = statistics.median(times)
            item[mode] = {
                "backend": samples[0]["backend"], "median_ms": median,
                "min_ms": min(times), "max_ms": max(times),
                "median_mib_per_second": count * size / 1024**2 / (median / 1000),
                "median_first_progress_ms": statistics.median(r["first_progress_ms"] for r in samples),
                "median_max_progress_gap_ms": statistics.median(r["max_progress_gap_ms"] for r in samples),
                "max_observed_progress_gap_ms": max(r["max_progress_gap_ms"] for r in samples),
                "intermediate_renders": sorted(set(r["intermediate_renders"] for r in samples)),
            }
        item["speedup"] = item["scalar"]["median_ms"] / item["simd"]["median_ms"]
        summary.append(item)
    metadata["utc_finished"] = datetime.now(timezone.utc).isoformat()
    (root / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    (root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
