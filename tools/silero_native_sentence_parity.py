#!/usr/bin/env python3
"""Fail-closed parity auditor for the native Silero sentence runtime."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


EXPECTED_IDS = (
    "exception_lookup",
    "oov",
    "homograph_selo_verb",
    "homograph_selo_noun",
    "yo_case",
    "punctuation_boundaries",
)
EXPECTED_ROUTES = {
    "exception_lookup": {"model": 2, "exception": 1, "phrase": 0, "homosolver": 0},
    "oov": {"model": 1, "exception": 0, "phrase": 0, "homosolver": 0},
    "homograph_selo_verb": {"model": 1, "exception": 0, "phrase": 1, "homosolver": 0},
    "homograph_selo_noun": {"model": 2, "exception": 0, "phrase": 0, "homosolver": 1},
    "yo_case": {"model": 2, "exception": 0, "phrase": 0, "homosolver": 0},
    "punctuation_boundaries": {"model": 1, "exception": 0, "phrase": 0, "homosolver": 1},
}
TRACE_PATTERN = re.compile(r"^TRACE model=(\d+);exception=(\d+);phrase=(\d+);homosolver=(\d+)$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--stress", required=True, type=Path)
    parser.add_argument("--yo", required=True, type=Path)
    parser.add_argument("--homo", required=True, type=Path)
    parser.add_argument("--vectors", type=Path, default=Path("docs/research/silero-phase1-vectors.jsonl"))
    parser.add_argument("--receipt", required=True, type=Path)
    return parser.parse_args()


def load_vectors(path: Path) -> list[dict]:
    vectors = []
    for line in path.read_text(encoding="utf-8").splitlines():
        record = json.loads(line)
        if record.get("record_type") == "vector":
            vectors.append(record)
    if tuple(vector["id"] for vector in vectors) != EXPECTED_IDS:
        raise SystemExit("Phase 1 vector order or membership changed")
    return vectors


def main() -> int:
    args = parse_args()
    vectors = load_vectors(args.vectors)
    command = [
        str(args.executable),
        "--assets",
        str(args.assets),
        "--stress",
        str(args.stress),
        "--yo",
        str(args.yo),
        "--homo",
        str(args.homo),
        "--trace",
    ]
    cases = []
    for vector in vectors:
        completed = subprocess.run(
            command,
            input=vector["input"].encode("utf-8"),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode != 0:
            raise SystemExit(f"native runtime failed for {vector['id']}: {completed.stderr.decode('utf-8', 'replace')}")
        expected_output = (vector["output"] + "\n").encode("utf-8")
        if completed.stdout != expected_output:
            raise SystemExit(f"native output parity failed for {vector['id']}")
        trace_lines = completed.stderr.decode("ascii", "strict").splitlines()
        if len(trace_lines) != 1:
            raise SystemExit(f"expected one trace line for {vector['id']}")
        match = TRACE_PATTERN.fullmatch(trace_lines[0])
        if match is None:
            raise SystemExit(f"invalid trace for {vector['id']}: {trace_lines[0]!r}")
        route = {
            "model": int(match.group(1)),
            "exception": int(match.group(2)),
            "phrase": int(match.group(3)),
            "homosolver": int(match.group(4)),
        }
        if route != EXPECTED_ROUTES[vector["id"]]:
            raise SystemExit(f"route parity failed for {vector['id']}: {route}")
        cases.append(
            {
                "id": vector["id"],
                "input": vector["input"],
                "reference_output": vector["output"],
                "native_output": completed.stdout.decode("utf-8").rstrip("\n"),
                "routes": route,
            }
        )
    manifest = args.assets / "manifest.json"
    receipt = {
        "record_type": "silero_native_sentence_parity",
        "vector_ids": list(EXPECTED_IDS),
        "full_native_call_parity": True,
        "route_parity": True,
        "native_executable_sha256": sha256(args.executable),
        "stress_onnx_sha256": sha256(args.stress),
        "yo_onnx_sha256": sha256(args.yo),
        "homosolver_onnx_sha256": sha256(args.homo),
        "asset_manifest_sha256": sha256(manifest),
        "cases": cases,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
