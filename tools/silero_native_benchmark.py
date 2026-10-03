"""Benchmark the native Silero runtime without changing its correctness corpus."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time
from typing import Iterable


CASES = {
    "ordinary": "\u041a\u0432\u0430\u043d\u0442\u043e\u043b\u0438\u043a.",
    "homograph": "\u042d\u0442\u043e \u0431\u043e\u043b\u044c\u0448\u043e\u0435 \u0441\u0435\u043b\u043e.",
    "phrase_hit": "\u0421\u043e\u043b\u043d\u0446\u0435 \u0441\u0435\u043b\u043e.",
    "mixed": "\u041c\u0430\u043c\u0430 \u043c\u044b\u043b\u0430 \u0440\u0430\u043c\u0443. \u0421\u043e\u043b\u043d\u0446\u0435 \u0441\u0435\u043b\u043e. \u0401\u043b\u043a\u0430 \u0438 \u0441\u0435\u043b\u043e\U0001f642.",
}


def percentile(samples: list[float], fraction: float) -> float:
    ordered = sorted(samples)
    return ordered[max(0, min(len(ordered) - 1, math.ceil(fraction * len(ordered)) - 1))]


def summarize(samples: list[float]) -> dict[str, float]:
    return {
        "count": len(samples),
        "min_ms": min(samples),
        "mean_ms": statistics.fmean(samples),
        "p50_ms": percentile(samples, 0.50),
        "p95_ms": percentile(samples, 0.95),
        "p99_ms": percentile(samples, 0.99),
        "max_ms": max(samples),
    }


def command(args: argparse.Namespace, mode: str | None = None) -> list[str]:
    result = [
        args.executable,
        "--assets",
        args.assets,
        "--stress",
        args.stress,
        "--yo",
        args.yo,
        "--homo",
        args.homo,
    ]
    if mode:
        result.append(mode)
    return result


def hash_files(paths: Iterable[Path]) -> tuple[int, dict[str, str], float]:
    hashes: dict[str, str] = {}
    total = 0
    started = time.perf_counter_ns()
    for path in sorted({path.resolve() for path in paths}):
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            while chunk := stream.read(4 * 1024 * 1024):
                digest.update(chunk)
                total += len(chunk)
        hashes[path.name] = digest.hexdigest()
    elapsed = (time.perf_counter_ns() - started) / 1_000_000
    return total, hashes, elapsed


def memory_bytes(pid: int) -> int | None:
    if os.name == "nt":
        class Counters(ctypes.Structure):
            _fields_ = [
                ("cb", ctypes.c_ulong),
                ("PageFaultCount", ctypes.c_ulong),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
            ]

        process_query = 0x0400 | 0x0010
        handle = ctypes.windll.kernel32.OpenProcess(process_query, False, pid)
        if not handle:
            return None
        try:
            counters = Counters()
            counters.cb = ctypes.sizeof(counters)
            if not ctypes.windll.psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
                return None
            return int(counters.WorkingSetSize)
        finally:
            ctypes.windll.kernel32.CloseHandle(handle)
    status = Path(f"/proc/{pid}/status")
    try:
        for line in status.read_text().splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1]) * 1024
    except (FileNotFoundError, ValueError):
        return None
    return None


def run_cold(cmd: list[str], sentence: str) -> float:
    started = time.perf_counter_ns()
    completed = subprocess.run(
        cmd,
        input=(sentence + "\n").encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=180,
        check=False,
    )
    elapsed = (time.perf_counter_ns() - started) / 1_000_000
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
    if not completed.stdout.endswith(b"\n"):
        raise RuntimeError("native benchmark process returned malformed output")
    return elapsed


def run_warm(cmd: list[str], sentences: list[str]) -> tuple[list[float], int | None]:
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    samples: list[float] = []
    peak_memory = memory_bytes(process.pid)
    try:
        assert process.stdin is not None
        assert process.stdout is not None
        for sentence in sentences:
            started = time.perf_counter_ns()
            process.stdin.write((sentence + "\n").encode("utf-8"))
            process.stdin.flush()
            output = process.stdout.readline()
            elapsed = (time.perf_counter_ns() - started) / 1_000_000
            if not output.endswith(b"\n"):
                raise RuntimeError("native benchmark process returned malformed output")
            samples.append(elapsed)
            current_memory = memory_bytes(process.pid)
            if current_memory is not None:
                peak_memory = max(peak_memory or 0, current_memory)
    finally:
        process.kill()
        _stdout, stderr = process.communicate()
        if process.returncode not in (0, -9, 1):
            raise RuntimeError(stderr.decode("utf-8", errors="replace"))
    return samples, peak_memory


def startup_breakdown(args: argparse.Namespace) -> dict[str, float]:
    started = time.perf_counter_ns()
    completed = subprocess.run(
        command(args, "--startup-trace"),
        input=b"\n",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=180,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
    values: dict[str, float] = {}
    for line in completed.stderr.decode("utf-8", errors="replace").splitlines():
        if line.startswith("STARTUP ") and "=" in line:
            name, value = line[8:].split("=", 1)
            values[name] = float(value)
    values["process_total_ms"] = (time.perf_counter_ns() - started) / 1_000_000
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--assets", required=True)
    parser.add_argument("--stress", required=True)
    parser.add_argument("--yo", required=True)
    parser.add_argument("--homo", required=True)
    parser.add_argument("--cold-runs", type=int, default=5)
    parser.add_argument("--warm-runs", type=int, default=30)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.cold_runs < 1 or args.warm_runs < 2:
        raise SystemExit("cold-runs must be >= 1 and warm-runs must be >= 2")

    root = Path(args.assets)
    graph_paths = [Path(args.stress), Path(args.yo), Path(args.homo)]
    asset_paths = list(root.iterdir()) + graph_paths
    bundle_size, asset_hashes, hash_ms = hash_files(asset_paths)
    _executable_size, executable_hashes, executable_hash_ms = hash_files([Path(args.executable)])
    cmd = command(args)

    cold_init = summarize([run_cold(cmd, "") for _ in range(args.cold_runs)])
    startup = startup_breakdown(args)
    cold_first = {
        name: summarize([run_cold(cmd, sentence) for _ in range(args.cold_runs)])
        for name, sentence in CASES.items()
    }
    warm: dict[str, dict[str, float]] = {}
    peak_memory: int | None = None
    for name, sentence in CASES.items():
        samples, observed_memory = run_warm(command(args, "--stream"), [sentence] * args.warm_runs)
        warm[name] = summarize(samples)
        if observed_memory is not None:
            peak_memory = max(peak_memory or 0, observed_memory)

    result = {
        "record_type": "silero_native_benchmark",
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "threads": 1,
        "cold_runs": args.cold_runs,
        "warm_runs": args.warm_runs,
        "bundle_size_bytes": bundle_size,
        "asset_count": len(asset_hashes),
        "asset_sha256": asset_hashes,
        "executable_sha256": executable_hashes[Path(args.executable).name],
        "executable_hash_elapsed_ms": executable_hash_ms,
        "hash_verification": {"elapsed_ms": hash_ms},
        "startup_breakdown": startup,
        "cold_initialization": cold_init,
        "cold_first_sentence": cold_first,
        "warm_sentence": warm,
        "peak_working_set_bytes": peak_memory,
    }
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
