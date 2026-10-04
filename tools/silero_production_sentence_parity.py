#!/usr/bin/env python3
"""Compare the private production sentence backend with the research runtime."""

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
TRACE_PATTERN = re.compile(r"^TRACE model=(\d+);exception=(\d+);phrase=(\d+);homosolver=(\d+)$")
WORD_PATTERN = re.compile(r"[А-Яа-яЁё+]+")
VOWELS = frozenset("АЕЁИОУЫЭЮЯаеёиоуыэюя")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--production", required=True, type=Path)
    parser.add_argument("--research", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--stress", required=True, type=Path)
    parser.add_argument("--yo", required=True, type=Path)
    parser.add_argument("--homo", required=True, type=Path)
    parser.add_argument("--vectors", type=Path, default=Path("docs/research/silero-phase1-vectors.jsonl"))
    parser.add_argument("--receipt", required=True, type=Path)
    return parser.parse_args()


def load_vectors(path: Path) -> list[dict]:
    vectors = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    vectors = [vector for vector in vectors if vector.get("record_type") == "vector"]
    if tuple(vector["id"] for vector in vectors) != EXPECTED_IDS:
        raise SystemExit("canonical sentence vector order or membership changed")
    return vectors


def research_stressed_vowels(output: str) -> list[int | None]:
    result: list[int | None] = []
    for word in WORD_PATTERN.findall(output):
        marker = word.find("+")
        if marker < 0:
            result.append(None)
        else:
            result.append(sum(char in VOWELS for char in word[:marker]))
    return result


def run(command: list[str], sentence: str, production: bool) -> tuple[str, dict[str, int], list[int | None]]:
    completed = subprocess.run(
        command,
        input=(sentence + "\n").encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise SystemExit(completed.stderr.decode("utf-8", "replace"))
    output = completed.stdout.decode("utf-8")
    trace_lines = completed.stderr.decode("ascii", "strict").splitlines()
    if not output.endswith("\n") or len(trace_lines) != (2 if production else 1):
        raise SystemExit("production/research probe did not emit the expected traces")
    match = TRACE_PATTERN.fullmatch(trace_lines[0])
    if match is None:
        raise SystemExit(f"invalid route trace: {trace_lines[0]!r}")
    routes = {name: int(match.group(index)) for index, name in enumerate(("model", "exception", "phrase", "homosolver"), 1)}
    if production:
        if not trace_lines[1].startswith("STRESSED"):
            raise SystemExit(f"missing stressed-vowel trace: {trace_lines[1]!r}")
        stressed = [None if value == "-" else int(value) for value in trace_lines[1].split()[1:]]
    else:
        stressed = research_stressed_vowels(output[:-1])
    return output[:-1], routes, stressed


def main() -> int:
    args = parse_args()
    vectors = load_vectors(args.vectors)
    production = [str(args.production), "--bundle", str(args.assets)]
    research = [
        str(args.research),
        "--assets", str(args.assets),
        "--stress", str(args.stress),
        "--yo", str(args.yo),
        "--homo", str(args.homo),
    ]
    cases = []
    for vector in vectors:
        production_output, production_routes, production_stressed = run(
            production, vector["input"], True
        )
        research_output, research_routes, research_stressed = run(
            research + ["--trace"], vector["input"], False
        )
        # The production contract carries stress as a vowel ordinal, not as a
        # renderer-specific '+' marker.  Compare the research spelling after
        # removing that marker while keeping every other UTF-8 byte intact.
        research_semantic_output = research_output.replace("+", "")
        if (
            production_output != research_semantic_output
            or production_routes != research_routes
            or production_stressed != research_stressed
        ):
            raise SystemExit(f"production/research parity failed for {vector['id']}")
        cases.append(
            {
                "id": vector["id"],
                "production_output": production_output,
                "research_output_without_markers": research_semantic_output,
                "routes": production_routes,
                "stressed_vowels": production_stressed,
            }
        )
    receipt = {
        "record_type": "silero_production_sentence_parity",
        "vector_ids": list(EXPECTED_IDS),
        "production_research_parity": True,
        "stress_marker_normalization": "research '+' markers removed; production uses stressed_vowel ordinal",
        "production_executable_sha256": sha256(args.production),
        "research_executable_sha256": sha256(args.research),
        "asset_manifest_sha256": sha256(args.assets / "manifest.json"),
        "cases": cases,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
