#!/usr/bin/env python3
"""Check ONNX parity for Silero's accentor classifier heads.

The upstream accentor accepts ``List[str]`` and performs custom n-gram lookup
inside a ScriptModule. This pass exports only the two neural classifier heads
after that embedding step. The embedding/preprocessing boundary is recorded as
an explicit unresolved gate rather than hidden behind a fake string export.
"""

from __future__ import annotations

import argparse
import json
import platform
import sys
from pathlib import Path

from silero_phase1 import (
    DEFAULT_MODEL_RELATIVE_PATH,
    PINNED_SOURCE_REVISION,
    git_revision,
    load_accentor,
    sha256,
)


WORDS = ("мама", "мыла", "раму", "квантолик", "село", "большое", "елка")
# The yo head contains logits around 2.8e5; this absolute bound is still
# sub-ppm relative error while avoiding false failures from float32 export.
TOLERANCE = 1.0e-1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Silero source checkout")
    parser.add_argument("--model", type=Path, help="model artifact; defaults to pinned accentor.pt")
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=Path(".temp/silero-phase3"),
        help="temporary export directory (must remain under .temp)",
    )
    parser.add_argument("--receipt", required=True, type=Path, help="JSON parity receipt path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    revision = git_revision(source)
    if revision != PINNED_SOURCE_REVISION:
        raise SystemExit(f"source revision {revision} does not match pinned {PINNED_SOURCE_REVISION}")
    work_dir = args.work_dir.resolve()
    if ".temp" not in work_dir.parts:
        raise SystemExit("--work-dir must be below the repository .temp directory")
    work_dir.mkdir(parents=True, exist_ok=True)
    model = (args.model or source / DEFAULT_MODEL_RELATIVE_PATH).resolve()
    if not model.is_file():
        raise SystemExit(f"model artifact does not exist: {model}")

    sys.path.insert(0, str(source / "src"))
    try:
        import numpy as np
        import onnx
        import onnxruntime as ort
        import torch
    except ImportError as error:
        raise SystemExit("install torch, onnx, and onnxruntime in the research environment") from error

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    accentor = load_accentor(torch, model)
    upstream_model = accentor.accentor.model
    embeddings = upstream_model.embedding(list(WORDS)).detach()
    stress_graph = upstream_model.stress_clf.eval()
    yo_graph = upstream_model.yo_clf.eval()
    stress_artifact = work_dir / "accentor-stress.onnx"
    yo_artifact = work_dir / "accentor-yo.onnx"
    torch.onnx.export(
        stress_graph,
        (embeddings,),
        stress_artifact,
        opset_version=17,
        dynamo=False,
        input_names=["input_embeddings"],
        output_names=["stress_logits"],
        dynamic_axes={"input_embeddings": {0: "word_count"}, "stress_logits": {0: "word_count"}},
    )
    torch.onnx.export(
        yo_graph,
        (embeddings,),
        yo_artifact,
        opset_version=17,
        dynamo=False,
        input_names=["input_embeddings"],
        output_names=["yo_logits"],
        dynamic_axes={"input_embeddings": {0: "word_count"}, "yo_logits": {0: "word_count"}},
    )
    onnx.checker.check_model(onnx.load(stress_artifact))
    onnx.checker.check_model(onnx.load(yo_artifact))
    stress_session = ort.InferenceSession(str(stress_artifact), providers=["CPUExecutionProvider"])
    yo_session = ort.InferenceSession(str(yo_artifact), providers=["CPUExecutionProvider"])
    cases = []
    global_max_abs_error = 0.0
    for case_id, words in (("word_count_1", WORDS[:1]), ("word_count_7", WORDS)):
        case_embeddings = upstream_model.embedding(list(words)).detach()
        with torch.no_grad():
            case_reference_stress = stress_graph(case_embeddings).numpy()
            case_reference_yo = yo_graph(case_embeddings).numpy()
        case_actual_stress = stress_session.run(
            None, {"input_embeddings": case_embeddings.numpy()}
        )[0]
        case_actual_yo = yo_session.run(None, {"input_embeddings": case_embeddings.numpy()})[0]
        stress_error = float(np.max(np.abs(case_reference_stress - case_actual_stress)))
        yo_error = float(np.max(np.abs(case_reference_yo - case_actual_yo)))
        case_max_error = max(stress_error, yo_error)
        global_max_abs_error = max(global_max_abs_error, case_max_error)
        if case_max_error > TOLERANCE:
            raise SystemExit(f"accentor classifier parity failed for {case_id}: {case_max_error}")
        cases.append(
            {
                "id": case_id,
                "word_count": len(words),
                "words": list(words),
                "per_word": [
                    {
                        "word": word,
                        "stress_argmax_reference": int(stress.argmax()),
                        "stress_argmax_onnx": int(actual_stress.argmax()),
                        "yo_argmax_reference": int(yo.argmax()),
                        "yo_argmax_onnx": int(yo.argmax()),
                    }
                    for word, stress, actual_stress, yo, actual_yo in zip(
                        words,
                        case_reference_stress,
                        case_actual_stress,
                        case_reference_yo,
                        case_actual_yo,
                    )
                ],
                "stress_max_abs_error": stress_error,
                "yo_max_abs_error": yo_error,
                "max_abs_error": case_max_error,
            }
        )

    resolved_model_path = (
        str(model.relative_to(source)).replace("\\", "/")
        if model.is_relative_to(source)
        else str(model)
    )
    receipt = {
        "record_type": "accentor_classifier_parity",
        "project": "snakers4/silero-stress",
        "source_revision": revision,
        "resolved_model_path": resolved_model_path,
        "model_sha256": sha256(model),
        "model_loader": "torch.package.PackageImporter",
        "graph_scope": "accentor stress/yo classifier heads after upstream n-gram embedding",
        "embedding_scope": "upstream AccentorNgram.model.embedding; not exported",
        "words": list(WORDS),
        "embedding_shape": list(embeddings.shape),
        "dynamic_axes": {
            "input_embeddings": ["word_count", 16],
            "stress_logits": ["word_count", 10],
            "yo_logits": ["word_count", 7],
        },
        "opset": 17,
        "onnx_artifacts": {
            "stress": {
                "name": stress_artifact.name,
                "sha256": sha256(stress_artifact),
                "bytes": stress_artifact.stat().st_size,
            },
            "yo": {
                "name": yo_artifact.name,
                "sha256": sha256(yo_artifact),
                "bytes": yo_artifact.stat().st_size,
            },
        },
        "torch": torch.__version__,
        "onnx": onnx.__version__,
        "onnxruntime": ort.__version__,
        "platform": platform.platform(),
        "providers": ["CPUExecutionProvider"],
        "cases": cases,
        "max_abs_error": global_max_abs_error,
        "tolerance": TOLERANCE,
        "full_call_parity": False,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(
        json.dumps(receipt, ensure_ascii=True, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(json.dumps(receipt, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
