#!/usr/bin/env python3
"""Fail-closed parity gate for the native Silero preprocessing prototype."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np

import silero_phase1
from silero_phase4_boundary import own_embeddings


TOLERANCE = 1.0e-5
WORDS = ("мама", "мыла", "раму", "квантолик", "село", "большое", "елка")
TOKEN_CASES = (
    ("homograph_selo_verb", "Солнце село.", [2, 40227, 1806, 18, 3]),
    ("homograph_selo_noun", "Это большое село.", [2, 5130, 15640, 1806, 18, 3]),
    (
        "marked_mel",
        "Мама, [HOMO] мел [/HOMO] !",
        [2, 37499, 16, 83828, 30468, 83829, 5, 3],
    ),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--preprocess-exe", required=True, type=Path)
    parser.add_argument("--wordpiece-exe", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    return parser.parse_args()


def run(executable: Path, arguments: list[str], lines: list[str]) -> list[str]:
    completed = subprocess.run(
        [str(executable), *arguments],
        input="\n".join(lines) + "\n",
        text=True,
        encoding="utf-8",
        capture_output=True,
        check=True,
    )
    return completed.stdout.splitlines()


def main() -> int:
    args = parse_args()
    manifest = json.loads((args.assets / "manifest.json").read_text(encoding="utf-8"))
    source = args.source.resolve()
    if silero_phase1.git_revision(source) != silero_phase1.PINNED_SOURCE_REVISION:
        raise SystemExit("source revision does not match pinned revision")
    try:
        import torch
    except ImportError as error:
        raise SystemExit("install torch in the research environment") from error
    accentor = silero_phase1.load_accentor(torch, source / silero_phase1.DEFAULT_MODEL_RELATIVE_PATH)
    embedding = accentor.accentor.model.embedding
    reference = embedding(list(WORDS)).detach().cpu().numpy()
    native_lines = run(
        args.preprocess_exe.resolve(),
        ["--ngrams", str(args.assets / "ngrams.tsv"), "--weights", str(args.assets / "embedding.f32")],
        list(WORDS),
    )
    native = np.asarray([[float(value) for value in line.split("\t")[1:]] for line in native_lines], dtype=np.float32)
    embedding_error = float(np.max(np.abs(reference - native)))
    if native.shape != reference.shape or embedding_error > TOLERANCE:
        raise SystemExit(f"native embedding parity failed: shape={native.shape} error={embedding_error}")

    token_lines = run(
        args.wordpiece_exe.resolve(), ["--vocab", str(args.assets / "bert-vocab.tsv")], [case[1] for case in TOKEN_CASES]
    )
    token_results = []
    for case, line in zip(TOKEN_CASES, token_lines):
        actual = [int(value) for value in line.rsplit("\t", 1)[1].split(",")]
        equal = actual == case[2]
        if not equal:
            raise SystemExit(f"native WordPiece parity failed for {case[0]}: {actual}")
        token_results.append({"id": case[0], "tokenizer_ids": actual, "parity": equal})

    receipt = {
        "record_type": "silero_native_preprocessing_parity",
        "source_revision": manifest["source_revision"],
        "model_sha256": manifest["model_sha256"],
        "asset_manifest": manifest,
        "embedding_words": list(WORDS),
        "embedding_shape": list(native.shape),
        "embedding_max_abs_error": embedding_error,
        "embedding_tolerance": TOLERANCE,
        "wordpiece_cases": token_results,
        "embedding_parity": True,
        "wordpiece_parity": True,
        "full_native_call_parity": False,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
