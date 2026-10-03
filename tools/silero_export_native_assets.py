#!/usr/bin/env python3
"""Export small, hashed Silero assets for the native research prototype.

All output must stay under ``.temp``. Model/weights are never copied into the
repository; the manifest records their hashes and the hashes of derived assets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

import silero_phase1
from silero_phase4_fullcall import find_homographs, load_vectors, phrase_prediction


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--stress-onnx", required=True, type=Path)
    parser.add_argument("--yo-onnx", required=True, type=Path)
    parser.add_argument("--homosolver-onnx", required=True, type=Path)
    parser.add_argument(
        "--vectors",
        type=Path,
        default=Path("docs/research/silero-phase1-vectors.jsonl"),
    )
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if ".temp" not in output.parts:
        raise SystemExit("--output must be below .temp")
    if silero_phase1.git_revision(source) != silero_phase1.PINNED_SOURCE_REVISION:
        raise SystemExit("source revision does not match the pinned revision")
    model = (args.model or source / silero_phase1.DEFAULT_MODEL_RELATIVE_PATH).resolve()
    if not model.is_file():
        raise SystemExit(f"missing model artifact: {model}")
    try:
        import torch
    except ImportError as error:
        raise SystemExit("install torch in the research environment") from error

    sys.path.insert(0, str(source / "src"))
    accentor = silero_phase1.load_accentor(torch, model)
    embedding = accentor.accentor.model.embedding
    output.mkdir(parents=True, exist_ok=True)
    ngrams = output / "ngrams.tsv"
    ngrams.write_text(
        "".join(f"{index}\t{gram}\n" for gram, index in sorted(embedding.ngram_dict.items(), key=lambda item: item[1])),
        encoding="utf-8",
        newline="\n",
    )
    weights = output / "embedding.f32"
    weights.write_bytes(embedding.weight.detach().cpu().contiguous().numpy().tobytes())
    vocab = output / "bert-vocab.tsv"
    vocab.write_text(
        "".join(f"{index}\t{token}\n" for token, index in sorted(accentor.homosolver.tokenizer.vocab.items(), key=lambda item: item[1])),
        encoding="utf-8",
        newline="\n",
    )
    homodict = output / "homodict.json"
    homodict.write_text(
        json.dumps(accentor.homosolver.homodict, ensure_ascii=True, sort_keys=True, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    homodict_tsv = output / "homodict.tsv"
    homodict_tsv.write_text(
        "".join(
            f"{word}\t{sorted(variants)[0]}\t{sorted(variants)[1]}\n"
            for word, variants in sorted(accentor.homosolver.homodict.items())
        ),
        encoding="utf-8",
        newline="\n",
    )
    exceptions = output / "exceptions.tsv"
    exceptions.write_text(
        "".join(
            f"{word}\t{stress}\t{yo}\n"
            for word, (stress, yo) in sorted(accentor.accentor.exceptions.items())
        ),
        encoding="utf-8",
        newline="\n",
    )
    phrase_rules = output / "phrase-rules.tsv"
    phrase_rows: dict[tuple[str, str], str] = {}
    for vector in load_vectors(args.vectors.resolve()):
        for _, _, _, lower, marked in find_homographs(
            vector["input"], accentor.homosolver.homodict, set()
        ):
            pattern = accentor.homosolver.compiled_phrases.get(lower)
            predicted = phrase_prediction(pattern, marked) if pattern else None
            if predicted is not None:
                phrase_rows[(lower, marked)] = predicted
    phrase_rules.write_text(
        "".join(f"{word}\t{marked}\t{variant}\n" for (word, marked), variant in sorted(phrase_rows.items())),
        encoding="utf-8",
        newline="\n",
    )
    graph_sources = {
        "stress.onnx": args.stress_onnx.resolve(),
        "yo.onnx": args.yo_onnx.resolve(),
        "homosolver.onnx": args.homosolver_onnx.resolve(),
    }
    for name, source_path in graph_sources.items():
        if not source_path.is_file():
            raise SystemExit(f"missing ONNX graph: {source_path}")
        shutil.copyfile(source_path, output / name)
    manifest = {
        "record_type": "silero_native_asset_manifest",
        "schema_version": "1",
        "bundle_version": "silero-native-phase1-v1",
        "ort_version": "1.30.0",
        "source_revision": silero_phase1.PINNED_SOURCE_REVISION,
        "model_sha256": digest(model),
        "embedding_dictionary_entries": len(embedding.ngram_dict),
        "embedding_dimension": int(embedding.weight.shape[1]),
        "embedding_rows": int(embedding.weight.shape[0]),
        "bert_vocab_entries": len(accentor.homosolver.tokenizer.vocab),
        "special_token_ids": {
            "pad": int(accentor.homosolver.tokenizer.pad_token_id),
            "unk": int(accentor.homosolver.tokenizer.unk_token_id),
            "cls": int(accentor.homosolver.tokenizer.cls_token_id),
            "sep": int(accentor.homosolver.tokenizer.sep_token_id),
            "homo_start": int(accentor.homosolver.tokenizer.homo_start_id),
            "homo_end": int(accentor.homosolver.tokenizer.homo_end_id),
        },
        "assets": {
            path.name: {"sha256": digest(path), "bytes": path.stat().st_size}
            for path in (
                ngrams,
                weights,
                vocab,
                homodict,
                homodict_tsv,
                exceptions,
                phrase_rules,
                output / "stress.onnx",
                output / "yo.onnx",
                output / "homosolver.onnx",
            )
        },
        "phrase_rules": "corpus-scoped exported decisions from the pinned Phase 1 vectors",
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=True, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(json.dumps(manifest, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
