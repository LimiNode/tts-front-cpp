#!/usr/bin/env python3
"""Fail-closed parity gate for the native ONNX Runtime execution probe."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np

import silero_phase1


ACCENTOR_TOLERANCE = 1.0e-5
HOMOGRAPH_TOLERANCE = 1.0e-4
WORDS = ("мама", "мыла", "раму", "квантолик", "село", "большое", "елка")
HOMOGRAPH_CASES = (
    ("homograph_selo_verb", [2, 40227, 1806, 18, 3], [1], [3]),
    ("homograph_selo_noun", [2, 5130, 15640, 1806, 18, 3], [2], [4]),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--stress-onnx", required=True, type=Path)
    parser.add_argument("--yo-onnx", required=True, type=Path)
    parser.add_argument("--homosolver-onnx", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_accentor_input(path: Path, values: np.ndarray) -> None:
    with path.open("w", encoding="ascii") as stream:
        stream.write(f"{values.shape[0]} {values.shape[1]}\n")
        stream.write(" ".join(f"{value:.9g}" for value in values.reshape(-1)))
        stream.write("\n")


def write_homograph_input(path: Path, ids: list[int], starts: list[int], ends: list[int]) -> None:
    with path.open("w", encoding="ascii") as stream:
        stream.write(f"1 {len(ids)}\n")
        stream.write(" ".join(str(value) for value in ids) + "\n")
        stream.write(" ".join(str(value) for value in starts) + "\n")
        stream.write(" ".join(str(value) for value in ends) + "\n")


def run_probe(probe: Path, graph: Path, kind: str, input_path: Path) -> np.ndarray:
    completed = subprocess.run(
        [str(probe.resolve()), "--graph", str(graph.resolve()), "--kind", kind, "--input", str(input_path)],
        text=True,
        encoding="utf-8",
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        raise SystemExit(f"native ORT probe failed: {completed.stderr.strip()}")
    lines = completed.stdout.splitlines()
    if len(lines) != 2 or not lines[0].startswith("shape=") or not lines[1].startswith("values="):
        raise SystemExit(f"malformed native ORT output: {completed.stdout!r}")
    shape = tuple(int(value) for value in lines[0][len("shape=") :].split(",") if value)
    values = np.asarray([float(value) for value in lines[1][len("values=") :].split(",")], dtype=np.float32)
    if not shape or int(np.prod(shape)) != values.size:
        raise SystemExit(f"native ORT output shape mismatch: {shape} vs {values.size}")
    return values.reshape(shape)


def compare_case(case_id: str, reference: np.ndarray, actual: np.ndarray, tolerance: float) -> dict[str, object]:
    if reference.shape != actual.shape:
        raise SystemExit(f"{case_id}: output shape mismatch {reference.shape} != {actual.shape}")
    error = float(np.max(np.abs(reference - actual)))
    reference_argmax = reference.argmax(axis=-1).tolist()
    actual_argmax = actual.argmax(axis=-1).tolist()
    if error > tolerance or reference_argmax != actual_argmax:
        raise SystemExit(
            f"{case_id}: parity failed error={error} argmax={reference_argmax}/{actual_argmax}"
        )
    return {
        "id": case_id,
        "shape": list(actual.shape),
        "max_abs_error": error,
        "tolerance": tolerance,
        "reference_argmax": reference_argmax,
        "native_argmax": actual_argmax,
        "parity": True,
    }


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    model = (args.model or source / silero_phase1.DEFAULT_MODEL_RELATIVE_PATH).resolve()
    artifacts = (model, args.stress_onnx, args.yo_onnx, args.homosolver_onnx, args.probe)
    for artifact in artifacts:
        if not artifact.is_file():
            raise SystemExit(f"missing artifact: {artifact}")
    if silero_phase1.git_revision(source) != silero_phase1.PINNED_SOURCE_REVISION:
        raise SystemExit("source revision does not match pinned revision")
    try:
        import onnxruntime as ort
        import torch
    except ImportError as error:
        raise SystemExit("install torch, numpy, and onnxruntime in the research environment") from error
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    accentor = silero_phase1.load_accentor(torch, model)
    embedding = accentor.accentor.model.embedding(list(WORDS)).detach().cpu().numpy().astype(np.float32)
    stress_session = ort.InferenceSession(str(args.stress_onnx), providers=["CPUExecutionProvider"])
    yo_session = ort.InferenceSession(str(args.yo_onnx), providers=["CPUExecutionProvider"])
    homo_session = ort.InferenceSession(str(args.homosolver_onnx), providers=["CPUExecutionProvider"])
    cases: list[dict[str, object]] = []
    with tempfile.TemporaryDirectory(prefix="silero-native-ort-") as temporary:
        root = Path(temporary)
        accentor_input = root / "accentor.txt"
        write_accentor_input(accentor_input, embedding)
        native_stress = run_probe(args.probe, args.stress_onnx, "accentor", accentor_input)
        native_yo = run_probe(args.probe, args.yo_onnx, "accentor", accentor_input)
        reference_stress = stress_session.run(None, {stress_session.get_inputs()[0].name: embedding})[0]
        reference_yo = yo_session.run(None, {yo_session.get_inputs()[0].name: embedding})[0]
        cases.append(compare_case("accentor_stress", reference_stress, native_stress, ACCENTOR_TOLERANCE))
        cases.append(compare_case("accentor_yo", reference_yo, native_yo, ACCENTOR_TOLERANCE))
        for case_id, ids, starts, ends in HOMOGRAPH_CASES:
            input_path = root / f"{case_id}.txt"
            write_homograph_input(input_path, ids, starts, ends)
            native = run_probe(args.probe, args.homosolver_onnx, "homograph", input_path)
            reference = homo_session.run(
                None,
                {
                    "input_ids": np.asarray([ids], dtype=np.int64),
                    "homo_start_ids": np.asarray(starts, dtype=np.int64),
                    "homo_end_ids": np.asarray(ends, dtype=np.int64),
                },
            )[0]
            cases.append(compare_case(case_id, reference, native, HOMOGRAPH_TOLERANCE))
    receipt = {
        "record_type": "silero_native_onnxruntime_parity",
        "source_revision": silero_phase1.git_revision(source),
        "model_sha256": sha256(model),
        "stress_onnx_sha256": sha256(args.stress_onnx),
        "yo_onnx_sha256": sha256(args.yo_onnx),
        "homosolver_onnx_sha256": sha256(args.homosolver_onnx),
        "onnxruntime": ort.__version__,
        "providers": ["CPUExecutionProvider"],
        "cases": cases,
        "output_argmax_parity": True,
        "full_native_call_parity": False,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
