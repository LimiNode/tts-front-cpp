#!/usr/bin/env python3
"""Compare the native phrase matcher with every exported upstream rule."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = json.loads((args.assets / "manifest.json").read_text(encoding="utf-8"))
    rows = [
        tuple(line.split("\t"))
        for line in (args.assets / "phrase-rules.tsv").read_text(encoding="utf-8").splitlines()
    ]
    if any(len(row) != 3 for row in rows):
        raise SystemExit("malformed phrase-rules.tsv")
    if manifest.get("phrase_rule_mismatches") != 0:
        raise SystemExit("phrase exporter reported mismatches")
    if manifest.get("phrase_rule_exported") != len(rows) or manifest.get("phrase_rule_entries") != len(rows):
        raise SystemExit("phrase manifest count does not match resource")
    if manifest.get("phrase_rule_precedence_overlaps", 0) < 1:
        raise SystemExit("precedence overlap regression is not represented")
    probe = [str(args.executable), "--assets", str(args.assets), "--phrase-probe-stdin"]
    payload = "".join(f"{word}\t{marked}\n" for word, marked, _ in rows).encode("utf-8")
    completed = subprocess.run(probe, input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if completed.returncode != 0:
        raise SystemExit(f"native phrase probe failed: {completed.stderr.decode('utf-8', 'replace')}")
    actual = completed.stdout.decode("utf-8").splitlines()
    expected = [variant for _, _, variant in rows]
    if actual != expected:
        for index, (native, reference) in enumerate(zip(actual, expected)):
            if native != reference:
                raise SystemExit(f"phrase parity failed at row {index}: {native!r} != {reference!r}")
        raise SystemExit(f"phrase parity output count mismatch: {len(actual)} != {len(expected)}")
    overlap_examples = manifest["phrase_rule_overlap_examples"]
    overlap_payload = "".join(f"{item['word']}\t{item['literal']}\n" for item in overlap_examples).encode("utf-8")
    overlap_run = subprocess.run(probe, input=overlap_payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if overlap_run.returncode != 0:
        raise SystemExit(f"native overlap probe failed: {overlap_run.stderr.decode('utf-8', 'replace')}")
    overlap_actual = overlap_run.stdout.decode("utf-8").splitlines()
    overlap_expected = [item["selected_variant"] for item in overlap_examples]
    if overlap_actual != overlap_expected:
        raise SystemExit(f"precedence overlap parity failed: {overlap_actual!r} != {overlap_expected!r}")
    print(f"phrase_rows={len(rows)} precedence_overlaps={manifest['phrase_rule_precedence_overlaps']} parity=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
