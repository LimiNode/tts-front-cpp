#!/usr/bin/env python3
"""Export small, hashed Silero assets for the native research prototype.

All output must stay under ``.temp``. Model/weights are never copied into the
repository; the manifest records their hashes and the hashes of derived assets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

import silero_phase1
from silero_phase4_fullcall import load_vectors


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def matching_parenthesis(text: str, start: int) -> int:
    depth = 0
    character_class = False
    escaped = False
    for index in range(start, len(text)):
        char = text[index]
        if escaped:
            escaped = False
            continue
        if char == "\\":
            escaped = True
            continue
        if character_class:
            if char == "]":
                character_class = False
            continue
        if char == "[":
            character_class = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unbalanced phrase regex")


def split_alternatives(text: str) -> list[str]:
    result = []
    begin = 0
    depth = 0
    character_class = False
    escaped = False
    for index, char in enumerate(text):
        if escaped:
            escaped = False
            continue
        if char == "\\":
            escaped = True
            continue
        if character_class:
            if char == "]":
                character_class = False
            continue
        if char == "[":
            character_class = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "|" and depth == 0:
            result.append(text[begin:index])
            begin = index + 1
    result.append(text[begin:])
    return result


def compiled_phrase_rules(compiled_phrases: dict) -> list[tuple[str, str, str]]:
    rows: set[tuple[str, str, str]] = set()
    for word, pattern in compiled_phrases.items():
        source = pattern.pattern
        cursor = 0
        while True:
            group_start = source.find("(?P<", cursor)
            if group_start == -1:
                break
            name_end = source.find(">", group_start + 4)
            if name_end == -1:
                raise ValueError("malformed named phrase group")
            group_end = matching_parenthesis(source, group_start)
            variant_name = source[group_start + 4 : name_end]
            inner = source[name_end + 1 : group_end]
            if inner.startswith("(?<!"):
                boundary_end = matching_parenthesis(inner, 0)
                inner = inner[boundary_end + 1 :]
            lookahead = inner.rfind("(?!")
            if lookahead != -1 and matching_parenthesis(inner, lookahead) == len(inner) - 1:
                inner = inner[:lookahead]
            if inner.startswith("(?:") and matching_parenthesis(inner, 0) == len(inner) - 1:
                inner = inner[3:-1]
            stressed = next(
                (
                    variant_name[:index] + "+" + variant_name[index:].lower()
                    for index, char in enumerate(variant_name)
                    if char.isupper()
                ),
                None,
            )
            if stressed is not None:
                for alternative in split_alternatives(inner):
                    escaped = False
                    for char in alternative:
                        if escaped:
                            escaped = False
                        elif char == "\\":
                            escaped = True
                        elif char in "()[]?*+{}^$|":
                            raise ValueError("unsupported non-literal compiled phrase construct")
                    literal = re.sub(r"\\(.)", r"\1", alternative)
                    match = pattern.search(literal)
                    selected = [] if match is None else [name for name in pattern.groupindex if match.group(name) is not None]
                    if literal and selected == [variant_name]:
                        rows.add((word, literal, stressed))
            cursor = group_end + 1
    return sorted(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--stress-onnx", required=True, type=Path)
    parser.add_argument("--yo-onnx", required=True, type=Path)
    parser.add_argument("--homosolver-onnx", required=True, type=Path)
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
    phrase_rows = compiled_phrase_rules(accentor.homosolver.compiled_phrases)
    phrase_rules.write_text(
        "".join(f"{word}\t{marked}\t{variant}\n" for word, marked, variant in phrase_rows),
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
        "phrase_rule_entries": len(phrase_rows),
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
        "phrase_rules": "full literal alternatives exported from pinned upstream compiled phrases",
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=True, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(json.dumps(manifest, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
