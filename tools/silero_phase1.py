#!/usr/bin/env python3
"""Capture deterministic Silero Stress Phase 1 reference vectors.

This is a research-only helper.  It deliberately lives outside the C++ build
and never downloads or copies model files into the repository.  Run it against
an exact checkout of the pinned upstream revision:

    python tools/silero_phase1.py \
        --source .temp/silero-source \
        --output docs/research/silero-phase1-vectors.jsonl
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import re
import subprocess
import sys
from pathlib import Path


PINNED_SOURCE_REVISION = "d38096cae9bf3ac846bbb88705428f3ed3801b96"
DEFAULT_MODEL_RELATIVE_PATH = "src/silero_stress/data/accentor.pt"

VECTORS = (
    {
        "id": "exception_lookup",
        "category": "known_vocabulary",
        "input": "\u041c\u0430\u043c\u0430 \u043c\u044b\u043b\u0430 \u0440\u0430\u043c\u0443.",
    },
    {
        "id": "oov",
        "category": "oov",
        "input": "\u041a\u0432\u0430\u043d\u0442\u043e\u043b\u0438\u043a.",
    },
    {
        "id": "homograph_selo_verb",
        "category": "homograph_context",
        "input": "\u0421\u043e\u043b\u043d\u0446\u0435 \u0441\u0435\u043b\u043e.",
        "expected_sense": "verb_past_neuter",
        "expected_stressed_vowel": 0,
    },
    {
        "id": "homograph_selo_noun",
        "category": "homograph_context",
        "input": "\u042d\u0442\u043e \u0431\u043e\u043b\u044c\u0448\u043e\u0435 \u0441\u0435\u043b\u043e.",
        "expected_sense": "noun_settlement",
        "expected_stressed_vowel": 1,
    },
    {
        "id": "yo_case",
        "category": "yo",
        "input": "\u0415\u043b\u043a\u0430 \u0451\u043b\u043a\u0430.",
    },
    {
        "id": "punctuation_boundaries",
        "category": "punctuation_boundary",
        "input": "\u00ab\u041c\u0430\u043c\u0430\u00bb, \u2014 \u043c\u0435\u043b!",
    },
)

VOWELS = frozenset("АЕЁИОУЫЭЮЯаеёиоуыэюя")
WORD_PATTERN = re.compile(r"[А-Яа-яЁё+]+")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_revision(source: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(source), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Silero source checkout")
    parser.add_argument(
        "--model",
        type=Path,
        help="model artifact (defaults to the pinned checkout's accentor.pt)",
    )
    parser.add_argument("--output", type=Path, help="JSONL receipt path (stdout by default)")
    parser.add_argument(
        "--allow-different-revision",
        action="store_true",
        help="allow a source checkout other than the pinned revision",
    )
    return parser.parse_args()


def load_accentor(torch, model: Path):
    """Load exactly *model*, mirroring upstream load_accentor post-processing."""
    with model.open("rb") as model_file:
        accentor = torch.package.PackageImporter(model_file).load_pickle(
            "accentor_models", "accentor"
        )
    quantized_weight = accentor.homosolver.model.bert.embeddings.word_embeddings.weight.data.clone()
    restored_weights = accentor.homosolver.model.bert.scale * (
        quantized_weight - accentor.homosolver.model.bert.zero_point
    )
    accentor.homosolver.model.bert.embeddings.word_embeddings.weight.data = restored_weights
    return accentor


def token_observations(accentor, text: str):
    raw_tokens, clean_tokens, prediction_mask = accentor.accentor._tokenize(text, set())
    observations = []
    cursor = 0
    homographs = set(accentor.homosolver.homodict) | set(accentor.homosolver.yohomodict)
    exceptions = accentor.accentor.exceptions
    for raw, clean, should_process in zip(raw_tokens, clean_tokens, prediction_mask):
        if not text.startswith(raw, cursor):
            raise RuntimeError(f"upstream tokenizer lost token boundary near offset {cursor}")
        start, end = cursor, cursor + len(raw)
        cursor = end
        if not clean:
            path = "separator"
        elif clean in homographs:
            path = "homograph_lookup"
        elif clean in exceptions:
            path = "exception_lookup"
        elif should_process:
            path = "accentor_model"
        else:
            path = "skipped"
        observations.append(
            {
                "raw": raw,
                "clean": clean,
                "process": bool(should_process),
                "path": path,
                "exception_hit": bool(clean and clean in exceptions),
                "homograph_hit": bool(clean and clean in homographs),
                "char_start": start,
                "char_end": end,
                "byte_start": len(text[:start].encode("utf-8")),
                "byte_end": len(text[:end].encode("utf-8")),
            }
        )
    if cursor != len(text):
        raise RuntimeError("upstream tokenizer did not cover the complete input")
    return observations


def add_neutral_stress(observations, output: str):
    word_matches = list(WORD_PATTERN.finditer(output))
    word_observations = [item for item in observations if item["clean"]]
    if len(word_matches) != len(word_observations):
        raise RuntimeError("could not align upstream output words to input tokens")
    for observation, match in zip(word_observations, word_matches):
        marked_word = match.group(0)
        marker = marked_word.find("+")
        observation["model_word"] = marked_word
        observation["stressed_vowel"] = (
            None if marker < 0 else sum(char in VOWELS for char in marked_word[:marker])
        )


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    revision = git_revision(source)
    if revision != PINNED_SOURCE_REVISION and not args.allow_different_revision:
        raise SystemExit(
            f"source revision {revision} does not match pinned {PINNED_SOURCE_REVISION}; "
            "use --allow-different-revision only for exploratory work"
        )

    model = (args.model or source / DEFAULT_MODEL_RELATIVE_PATH).resolve()
    if not model.is_file():
        raise SystemExit(f"model artifact does not exist: {model}")

    source_src = source / "src"
    sys.path.insert(0, str(source_src))
    try:
        import torch
    except ImportError as error:
        raise SystemExit("install the pinned Silero Python dependencies (torch)") from error

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    accentor = load_accentor(torch, model)

    records = [
        {
            "record_type": "metadata",
            "project": "snakers4/silero-stress",
            "source_revision": revision,
            "resolved_model_path": str(model.relative_to(source)).replace("\\", "/")
            if model.is_relative_to(source)
            else str(model),
            "model_sha256": sha256(model),
            "model_loader": "torch.package.PackageImporter",
            "python": platform.python_version(),
            "torch": torch.__version__,
            "platform": platform.platform(),
            "torch_num_threads": torch.get_num_threads(),
            "torch_num_interop_threads": torch.get_num_interop_threads(),
            "stress_marker": "+",
            "output_contract": "SileroStress.__call__ output with + stress markers",
            "neutral_stress_contract": "per-token zero-based stressed_vowel ordinal",
            "determinism_repeats": 2,
        }
    ]
    captures = []
    for _ in range(2):
        current = []
        for vector in VECTORS:
            output = accentor(vector["input"])
            observations = token_observations(accentor, vector["input"])
            add_neutral_stress(observations, output)
            current.append({**vector, "record_type": "vector", "output": output, "tokens": observations})
        captures.append(current)
    if captures[0] != captures[1]:
        raise SystemExit("Silero reference output was not deterministic across two repeats")
    records.extend(captures[0])

    serialized = "".join(
        json.dumps(record, ensure_ascii=True, sort_keys=True) + "\n" for record in records
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(serialized)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
