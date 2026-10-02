#!/usr/bin/env python3
"""Run the sentence-level Silero hybrid pipeline against Phase 1 vectors.

The accentor tokenizer, n-gram embedding, and classifier heads are reproduced
locally. Homograph spans are prepared from the pinned homograph dictionary and
run through the pinned Phase 2 ONNX graph. The remaining tokenizer used inside
the BERT graph is intentionally recorded as an upstream tokenizer dependency;
this is the last Python boundary before the native prototype.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import numpy as np
import silero_phase1
from silero_phase4_boundary import load_vectors, own_embeddings, own_tokenize, sha256


HOMO_PATTERN = re.compile(r"(?=.*[а-яё])[а-яё+]+", re.IGNORECASE)
VOWELS = "аоуыэиеяёю"
STRESS_TOKEN = "+"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--stress-onnx", required=True, type=Path)
    parser.add_argument("--yo-onnx", required=True, type=Path)
    parser.add_argument("--homosolver-onnx", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--vectors", type=Path, default=Path("docs/research/silero-phase1-vectors.jsonl"))
    parser.add_argument("--model", type=Path)
    parser.add_argument("--work-dir", type=Path, default=Path(".temp/silero-phase4-fullcall"))
    return parser.parse_args()


def clean_context(text: str, is_start: bool) -> str:
    text = re.sub(r"[^a-zA-Zа-яА-ЯёЁ0-9\s.!?,\-]", "", text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"-{2,}", " - ", text)
    text = re.sub(r"([.!?])\1+", r"\1", text)
    text = re.sub(r",{2,}", ",", text)
    text = re.sub(r"\s+([.,!?])", r"\1", text)
    text = re.sub(r"([.!?])\1+", r"\1", text)
    text = re.sub(r",{2,}", ",", text)
    text = re.sub(r"([.,!?])(?=\S)", r"\1 ", text)
    text = re.sub(r"\s+", " ", text).strip()
    if is_start:
        text = text.lstrip(" .,!?-")
        return text[:1].upper() + text[1:].lower() if text else ""
    text = text[:1] + text[1:].lower() if text else ""
    return text if not text or text[-1] in ".!?" else text + "."


def find_homographs(sentence: str, homodict: dict[str, list[str]], ignored: set[str]):
    tagged = []
    for match in HOMO_PATTERN.finditer(sentence):
        start, end = match.span()
        word = match.group()
        lower = word.lower()
        if lower not in homodict or lower in ignored:
            continue
        left = clean_context(sentence[:start], True)[-150:]
        right = clean_context(sentence[end:], False)[:150]
        marked = (left + " [HOMO] " + lower + " [/HOMO] " + right).strip()
        tagged.append((start, end, word, lower, marked))
    return tagged


def phrase_prediction(pattern, marked: str):
    match = pattern.search(marked)
    if not match:
        return None
    for group_name, matched in match.groupdict().items():
        if matched is not None:
            for index, char in enumerate(group_name):
                if char.isupper():
                    return group_name[:index] + STRESS_TOKEN + group_name[index:].lower()
    return None


def run_homosolver(sentence, accentor, session, put_yo=True, put_stress=True):
    tagged = find_homographs(sentence, accentor.homosolver.homodict, set())
    if not tagged:
        return sentence, []
    tokenizer = accentor.homosolver.tokenizer
    variants = [None] * len(tagged)
    paths = [None] * len(tagged)
    neural_indices = []
    encoded = []
    starts = []
    ends = []
    for index, (_, _, _, lower, marked) in enumerate(tagged):
        pattern = accentor.homosolver.compiled_phrases.get(lower)
        predicted = phrase_prediction(pattern, marked) if pattern else None
        if predicted is not None:
            variants[index] = predicted
            paths[index] = "phrase_rule"
            continue
        ids = np.asarray(tokenizer(marked), dtype=np.int64)
        neural_indices.append(index)
        encoded.append(ids)
        starts.append(int(np.flatnonzero(ids == tokenizer.homo_start_id)[0]))
        ends.append(int(np.flatnonzero(ids == tokenizer.homo_end_id)[0]))
    if encoded:
        width = max(len(ids) for ids in encoded)
        batch = np.full((len(encoded), width), tokenizer.pad_token_id, dtype=np.int64)
        for index, ids in enumerate(encoded):
            batch[index, : len(ids)] = ids
        logits = session.run(
            None,
            {
                "input_ids": batch,
                "homo_start_ids": np.asarray(starts, dtype=np.int64),
                "homo_end_ids": np.asarray(ends, dtype=np.int64),
            },
        )[0]
        for index, row in zip(neural_indices, logits):
            lower = tagged[index][3]
            prediction = int(float(1.0 / (1.0 + np.exp(-row[0]))) >= 0.5)
            variants[index] = sorted(accentor.homosolver.homodict[lower])[prediction]
            paths[index] = "homosolver_onnx"
    output = sentence
    offset = 0
    for (start, end, word, _, _), variant in zip(tagged, variants):
        if not put_yo:
            variant = variant.replace("ё", "е")
        stress_index = variant.index(STRESS_TOKEN)
        variant = variant.replace(STRESS_TOKEN, "")
        variant = "".join(
            new.lower() if old.islower() else new.upper()
            for old, new in zip(word, variant)
        )
        vowel_count = sum(char.lower() in VOWELS for char in variant)
        start += offset
        end += offset
        if put_stress:
            variant = variant[:stress_index] + STRESS_TOKEN + variant[stress_index:]
            offset += 1
        output = output[:start] + variant + output[end:]
    return output, [
        {"word": item[2], "variant": variant, "path": path}
        for item, variant, path in zip(tagged, variants, paths)
    ]


def positions(word: str, stressed_ids, yo_ids):
    vowels = [index for index, char in enumerate(word) if char in VOWELS]
    yes = [index for index, char in enumerate(word) if char == "е"]
    stress_positions = [vowels[index] for index in stressed_ids if index < len(vowels)]
    yo_positions = [yes[index - 1] for index in yo_ids if index > 0 and index - 1 < len(yes)]
    return stress_positions, yo_positions, len(vowels), vowels[0] if vowels else -1


def accentuate(sentence, accentor, stress_session, yo_session, embedding, ngram_dict, weight):
    raw_tokens, clean_tokens, masks = own_tokenize(sentence)
    embeddings = own_embeddings(clean_tokens, ngram_dict, weight)
    stress_logits = stress_session.run(None, {"input_embeddings": embeddings})[0]
    yo_logits = yo_session.run(None, {"input_embeddings": embeddings})[0]
    stress_probs = np.exp(stress_logits - stress_logits.max(axis=1, keepdims=True))
    stress_probs /= stress_probs.sum(axis=1, keepdims=True)
    yo_probs = np.exp(yo_logits - yo_logits.max(axis=1, keepdims=True))
    yo_probs /= yo_probs.sum(axis=1, keepdims=True)
    stress_preds = stress_logits.argmax(axis=1)
    yo_preds = yo_logits.argmax(axis=1)
    exceptions = accentor.accentor.exceptions
    output = []
    for index, (raw_word, clean_word, process) in enumerate(zip(raw_tokens, clean_tokens, masks)):
        lower = raw_word.lower()
        if not process:
            output.append(raw_word)
            continue
        have_stress = STRESS_TOKEN in lower
        have_yo = "ё" in lower
        if have_stress and have_yo:
            output.append(raw_word)
            continue
        if not have_stress and have_yo:
            yo_positions = [position for position, char in enumerate(lower) if char == "ё"]
            for position, yo_position in enumerate(yo_positions):
                raw_word = raw_word[: yo_position + position] + STRESS_TOKEN + raw_word[yo_position + position :]
            output.append(raw_word)
            continue
        if clean_word in exceptions:
            stress_index, yo_index = exceptions[clean_word]
            if not have_stress:
                if yo_index != -1:
                    raw_word = raw_word[:yo_index] + ("ё" if raw_word[yo_index].islower() else "Ё") + raw_word[yo_index + 1 :]
                raw_word = raw_word[:stress_index] + STRESS_TOKEN + raw_word[stress_index:]
            output.append(raw_word)
            continue
        stress_id = int(stress_preds[index])
        yo_id = int(yo_preds[index])
        set_stress = stress_probs[index, stress_id] > 0.5 and not have_stress
        set_yo = yo_probs[index, yo_id] > 0.5
        if have_stress:
            stress_ids = [
                sum(part.count(char) for char in VOWELS)
                for part in lower.split(STRESS_TOKEN)
            ]
        else:
            stress_ids = [stress_id]
        stress_positions, yo_positions, vowel_count, first_vowel = positions(lower, stress_ids, [yo_id])
        if not vowel_count:
            output.append(raw_word)
            continue
        for yo_position in yo_positions:
            if yo_position in stress_positions and set_yo and raw_word_lower_is_e(raw_word, yo_position):
                raw_word = raw_word[:yo_position] + ("ё" if raw_word[yo_position].islower() else "Ё") + raw_word[yo_position + 1 :]
        if vowel_count == 1:
            stress_positions = [first_vowel]
            set_stress = True
        if not have_stress and set_stress:
            for shift, stress_position in enumerate(stress_positions):
                raw_word = raw_word[: stress_position + shift] + STRESS_TOKEN + raw_word[stress_position + shift :]
        output.append(raw_word)
    return "".join(output)


def raw_word_lower_is_e(word: str, position: int) -> bool:
    return word[position].lower() == "е"


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    if silero_phase1.git_revision(source) != silero_phase1.PINNED_SOURCE_REVISION:
        raise SystemExit("source revision does not match pinned revision")
    model = (args.model or source / silero_phase1.DEFAULT_MODEL_RELATIVE_PATH).resolve()
    for artifact in (model, args.stress_onnx, args.yo_onnx, args.homosolver_onnx):
        if not artifact.is_file():
            raise SystemExit(f"missing artifact: {artifact}")
    try:
        import onnxruntime as ort
        import torch
    except ImportError as error:
        raise SystemExit("install torch and onnxruntime in the research environment") from error
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    sys.path.insert(0, str(source / "src"))
    accentor = silero_phase1.load_accentor(torch, model)
    embedding = accentor.accentor.model.embedding
    ngram_dict = dict(embedding.ngram_dict)
    weight = embedding.weight.detach().cpu().numpy()
    stress_session = ort.InferenceSession(str(args.stress_onnx), providers=["CPUExecutionProvider"])
    yo_session = ort.InferenceSession(str(args.yo_onnx), providers=["CPUExecutionProvider"])
    homo_session = ort.InferenceSession(str(args.homosolver_onnx), providers=["CPUExecutionProvider"])
    cases = []
    for vector in load_vectors(args.vectors.resolve()):
        homo_output, decisions = run_homosolver(vector["input"], accentor, homo_session)
        hybrid_output = accentuate(homo_output, accentor, stress_session, yo_session, embedding, ngram_dict, weight)
        equal = hybrid_output == vector["output"]
        if not equal:
            raise SystemExit(f"full-call parity failed for {vector['id']}: {hybrid_output!r}")
        cases.append(
            {
                "id": vector["id"],
                "input": vector["input"],
                "reference_output": vector["output"],
                "hybrid_output": hybrid_output,
                "exact_output_parity": equal,
                "homograph_decisions": decisions,
            }
        )
    receipt = {
        "record_type": "accentor_sentence_orchestration_parity",
        "project": "snakers4/silero-stress",
        "source_revision": silero_phase1.git_revision(source),
        "model_sha256": sha256(model),
        "stress_onnx_sha256": sha256(args.stress_onnx),
        "yo_onnx_sha256": sha256(args.yo_onnx),
        "homosolver_onnx_sha256": sha256(args.homosolver_onnx),
        "homograph_tokenizer": "pinned upstream SimpleBertTokenizer",
        "upstream_boundary_dependencies": [
            "SimpleBertTokenizer",
            "compiled homograph phrase rules",
        ],
        "parity_scope": "Python/ONNX hybrid final sentence output",
        "cases": cases,
        "exact_output_parity": True,
        "full_call_parity": True,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
