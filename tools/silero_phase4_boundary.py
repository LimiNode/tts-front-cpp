#!/usr/bin/env python3
"""Audit the reproducible preprocessing boundary for Silero's accentor.

This research-only helper deliberately reimplements the upstream tokenizer and
FastText-style n-gram embedding instead of calling the ScriptModule for those
steps.  It then feeds the resulting embeddings to the Phase 3 ONNX classifier
artifacts and compares both embeddings and classifier decisions with PyTorch.
The sentence-level homograph cascade is recorded as an explicit unresolved
boundary; this tool does not claim full ``SileroStress.__call__`` parity.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import re
import sys
from pathlib import Path

import silero_phase1


PINNED_SOURCE_REVISION = silero_phase1.PINNED_SOURCE_REVISION
WORD_SPLIT = re.compile(r"([\s.,!?;:<>=()/\\]+)")
RE_COND = re.compile(r"[^А-Яа-яёЁ]")
EMBEDDING_TOLERANCE = 1.0e-5
TARGETED_TOKENIZATION_CASES = (
    {"id": "hyphenated_word", "input": "\u043a\u0442\u043e-\u043b\u0438\u0431\u043e", "words_to_ignore": []},
    {"id": "hyphen_to", "input": "\u0447\u0442\u043e-\u0442\u043e", "words_to_ignore": []},
    {
        "id": "words_to_ignore",
        "input": "\u043c\u0430\u043c\u0430 \u043f\u0430\u043f\u0430",
        "words_to_ignore": ["\u043c\u0430\u043c\u0430"],
    },
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--stress-onnx", required=True, type=Path)
    parser.add_argument("--yo-onnx", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument(
        "--vectors",
        type=Path,
        default=Path("docs/research/silero-phase1-vectors.jsonl"),
    )
    parser.add_argument("--model", type=Path)
    parser.add_argument("--work-dir", type=Path, default=Path(".temp/silero-phase4"))
    return parser.parse_args()


def own_tokenize(sentence: str, words_to_ignore: set[str] | None = None):
    ignored = words_to_ignore or set()
    raw_tokens: list[str] = []
    clean_tokens: list[str] = []
    prediction_mask: list[bool] = []
    for word in WORD_SPLIT.split(sentence):
        parts = word.split("-")
        if len(parts) == 1:
            current = parts
            masks = [True]
        else:
            current = [part + "-" for part in parts[:-1]] + [parts[-1]]
            masks = [True for _ in parts[:-1]] + [parts[-1] != "то"]
        clean = [RE_COND.sub("", token.lower()) for token in current]
        masks = [bool(value) and mask and value not in ignored for value, mask in zip(clean, masks)]
        raw_tokens.extend(current)
        clean_tokens.extend(clean)
        prediction_mask.extend(masks)
    return raw_tokens, clean_tokens, prediction_mask


def word_ngrams(word: str) -> list[str]:
    padded = "<" + word + ">"
    grams: list[str] = []
    for size in range(1, len(word) + 4):
        grams.extend(padded[index : index + size] for index in range(len(padded) - size + 1))
    return grams


def own_embeddings(words, ngram_dict, weight):
    import numpy as np

    rows = []
    for word in words:
        indices = [ngram_dict[gram] for gram in word_ngrams(word) if gram in ngram_dict]
        if not indices:
            indices = [ngram_dict["UNK"]]
        rows.append(weight[indices].mean(axis=0))
    return np.asarray(rows, dtype=np.float32)


def load_vectors(path: Path):
    return [
        json.loads(line)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line and json.loads(line).get("record_type") == "vector"
    ]


def artifact_hash(artifact: Path) -> dict[str, object]:
    return {"name": artifact.name, "sha256": sha256(artifact), "bytes": artifact.stat().st_size}


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    if silero_phase1.git_revision(source) != PINNED_SOURCE_REVISION:
        raise SystemExit("source revision does not match the pinned Silero revision")
    work_dir = args.work_dir.resolve()
    if ".temp" not in work_dir.parts:
        raise SystemExit("--work-dir must be below .temp")
    model = (args.model or source / silero_phase1.DEFAULT_MODEL_RELATIVE_PATH).resolve()
    if not model.is_file() or not args.stress_onnx.is_file() or not args.yo_onnx.is_file():
        raise SystemExit("model and both Phase 3 ONNX artifacts must exist")

    try:
        import numpy as np
        import onnxruntime as ort
        import torch
    except ImportError as error:
        raise SystemExit("install torch, numpy, and onnxruntime in the research environment") from error

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    sys.path.insert(0, str(source / "src"))
    accentor = silero_phase1.load_accentor(torch, model)
    upstream = accentor.accentor.model
    embedding = upstream.embedding
    ngram_dict = dict(embedding.ngram_dict)
    weight = embedding.weight.detach().cpu().numpy()
    stress_session = ort.InferenceSession(str(args.stress_onnx), providers=["CPUExecutionProvider"])
    yo_session = ort.InferenceSession(str(args.yo_onnx), providers=["CPUExecutionProvider"])
    vectors = load_vectors(args.vectors.resolve())
    homographs = set(accentor.homosolver.homodict) | set(accentor.homosolver.yohomodict)
    exceptions = accentor.accentor.exceptions
    cases = []
    targeted_cases = []
    max_embedding_error = 0.0
    all_classifier_argmax_equal = True
    all_tokenization_equal = True

    for targeted in TARGETED_TOKENIZATION_CASES:
        ignored = set(targeted["words_to_ignore"])
        own = own_tokenize(targeted["input"], ignored)
        reference = accentor.accentor._tokenize(targeted["input"], ignored)
        equal = own == reference
        if not equal:
            raise SystemExit(f"targeted tokenization parity failed for {targeted['id']}")
        targeted_cases.append(
            {
                **targeted,
                "tokenization_equal": equal,
                "raw_tokens": own[0],
                "clean_tokens": own[1],
                "prediction_mask": own[2],
            }
        )

    for vector in vectors:
        sentence = vector["input"]
        own_raw, own_clean, own_mask = own_tokenize(sentence)
        ref_raw, ref_clean, ref_mask = accentor.accentor._tokenize(sentence, None)
        tokenization_equal = (own_raw, own_clean, own_mask) == (ref_raw, ref_clean, ref_mask)
        if not tokenization_equal:
            raise SystemExit(f"tokenization parity failed for {vector['id']}")
        all_tokenization_equal &= tokenization_equal
        own = own_embeddings(own_clean, ngram_dict, weight)
        with torch.no_grad():
            reference = embedding(ref_clean).detach().cpu().numpy()
            reference_stress = upstream.stress_clf(embedding(ref_clean)).detach().cpu().numpy()
            reference_yo = upstream.yo_clf(embedding(ref_clean)).detach().cpu().numpy()
        actual_stress = stress_session.run(None, {"input_embeddings": own})[0]
        actual_yo = yo_session.run(None, {"input_embeddings": own})[0]
        embedding_error = float(np.max(np.abs(reference - own)))
        stress_equal = np.array_equal(reference_stress.argmax(1), actual_stress.argmax(1))
        yo_equal = np.array_equal(reference_yo.argmax(1), actual_yo.argmax(1))
        if not stress_equal:
            raise SystemExit(f"stress argmax parity failed for {vector['id']}")
        if not yo_equal:
            raise SystemExit(f"yo argmax parity failed for {vector['id']}")
        if embedding_error > EMBEDDING_TOLERANCE:
            raise SystemExit(
                f"embedding parity failed for {vector['id']}: {embedding_error}"
            )
        all_classifier_argmax_equal &= stress_equal and yo_equal
        max_embedding_error = max(max_embedding_error, embedding_error)
        paths = []
        for clean, process in zip(own_clean, own_mask):
            if not clean:
                path = "separator"
            elif clean in homographs:
                path = "homograph_lookup"
            elif clean in exceptions:
                path = "exception_lookup"
            elif process:
                path = "accentor_model"
            else:
                path = "skipped"
            paths.append({"clean": clean, "process": bool(process), "path": path})
        cases.append(
            {
                "id": vector["id"],
                "input": sentence,
                "reference_output": vector["output"],
                "tokenization_equal": tokenization_equal,
                "embedding_shape": list(own.shape),
                "embedding_max_abs_error": embedding_error,
                "stress_argmax_equal": stress_equal,
                "yo_argmax_equal": yo_equal,
                "paths": paths,
                "homograph_resolution": "delegated_to_upstream_reference"
                if any(item["path"] == "homograph_lookup" for item in paths)
                else "not_needed",
            }
        )

    ngram_serialized = "".join(f"{key}\0{value}\0" for key, value in sorted(ngram_dict.items()))
    ngram_hash = hashlib.sha256(ngram_serialized.encode("utf-8")).hexdigest()
    receipt = {
        "record_type": "accentor_preprocessing_boundary",
        "project": "snakers4/silero-stress",
        "source_revision": silero_phase1.git_revision(source),
        "resolved_model_path": str(model.relative_to(source)).replace("\\", "/")
        if model.is_relative_to(source)
        else str(model),
        "model_sha256": sha256(model),
        "model_loader": "torch.package.PackageImporter",
        "embedding_algorithm": "word_ngrams(1..len(word)+3) + mean embedding_bag",
        "embedding_dictionary_entries": len(ngram_dict),
        "embedding_dictionary_sha256": ngram_hash,
        "embedding_weight_shape": list(weight.shape),
        "embedding_weight_sha256": hashlib.sha256(weight.tobytes()).hexdigest(),
        "stress_onnx": artifact_hash(args.stress_onnx.resolve()),
        "yo_onnx": artifact_hash(args.yo_onnx.resolve()),
        "torch": torch.__version__,
        "onnxruntime": ort.__version__,
        "platform": platform.platform(),
        "providers": ["CPUExecutionProvider"],
        "tokenization_parity": all_tokenization_equal,
        "targeted_tokenization_parity": True,
        "targeted_tokenization_cases": targeted_cases,
        "classifier_argmax_parity": all_classifier_argmax_equal,
        "max_embedding_abs_error": max_embedding_error,
        "embedding_tolerance": EMBEDDING_TOLERANCE,
        "full_call_parity": False,
        "homograph_resolution": "delegated_to_upstream_reference",
        "cases": cases,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
