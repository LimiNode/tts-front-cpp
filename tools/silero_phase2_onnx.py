#!/usr/bin/env python3
"""Export and check ONNX parity for the pinned Silero homosolver graph.

This is a research-only graph-level gate.  It exports the neural
``homosolver.model`` from the exact Phase 1 artifact; tokenizer logic,
homograph lookup tables, phrase rules, and the n-gram accentor remain outside
the graph and are not silently presented as exported here.
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


SENTENCE = "\u0421\u043e\u043b\u043d\u0446\u0435 \u0441\u0435\u043b\u043e."
TOLERANCE = 1.0e-4


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Silero source checkout")
    parser.add_argument("--model", type=Path, help="model artifact; defaults to pinned accentor.pt")
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=Path(".temp/silero-phase2"),
        help="temporary export directory (must remain under .temp)",
    )
    parser.add_argument("--receipt", required=True, type=Path, help="JSON parity receipt path")
    parser.add_argument(
        "--allow-different-revision",
        action="store_true",
        help="allow a source checkout other than the pinned revision",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    revision = git_revision(source)
    if revision != PINNED_SOURCE_REVISION and not args.allow_different_revision:
        raise SystemExit(
            f"source revision {revision} does not match pinned {PINNED_SOURCE_REVISION}"
        )

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
        raise SystemExit(
            "install torch, onnx, and onnxruntime in the research environment"
        ) from error

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    accentor = load_accentor(torch, model)
    graph = accentor.homosolver.model.eval()

    # The upstream homosolver uses a marker position immediately before the
    # selected word.  For "Солнце село." the tokenizer is [CLS, Солнце, село,
    # ., SEP], hence marker index 1 and exclusive end index 3.
    input_ids = torch.tensor([accentor.homosolver.tokenizer(SENTENCE)], dtype=torch.long)
    homo_start_ids = torch.tensor([1], dtype=torch.long)
    homo_end_ids = torch.tensor([3], dtype=torch.long)
    artifact = work_dir / "homosolver.onnx"

    with torch.no_grad():
        reference = graph(input_ids, homo_start_ids, homo_end_ids).cpu().numpy()
    torch.onnx.export(
        graph,
        (input_ids, homo_start_ids, homo_end_ids),
        artifact,
        opset_version=17,
        dynamo=False,
        input_names=["input_ids", "homo_start_ids", "homo_end_ids"],
        output_names=["logits"],
    )
    onnx.checker.check_model(onnx.load(artifact))
    session = ort.InferenceSession(str(artifact), providers=["CPUExecutionProvider"])
    actual = session.run(
        None,
        {
            "input_ids": input_ids.numpy(),
            "homo_start_ids": homo_start_ids.numpy(),
            "homo_end_ids": homo_end_ids.numpy(),
        },
    )[0]
    max_abs_error = float(np.max(np.abs(reference - actual)))
    if max_abs_error > TOLERANCE:
        raise SystemExit(f"ONNX parity failed: max_abs_error={max_abs_error}")

    resolved_model_path = (
        str(model.relative_to(source)).replace("\\", "/")
        if model.is_relative_to(source)
        else str(model)
    )
    receipt = {
        "record_type": "onnx_graph_parity",
        "project": "snakers4/silero-stress",
        "source_revision": revision,
        "resolved_model_path": resolved_model_path,
        "model_sha256": sha256(model),
        "model_loader": "torch.package.PackageImporter",
        "graph_scope": "homosolver.model only; tokenizer and lookup tables remain outside graph",
        "sentence": SENTENCE,
        "tokenizer_ids": input_ids[0].tolist(),
        "homo_start_ids": homo_start_ids.tolist(),
        "homo_end_ids": homo_end_ids.tolist(),
        "opset": 17,
        "onnx_artifact": artifact.name,
        "onnx_sha256": sha256(artifact),
        "onnx_bytes": artifact.stat().st_size,
        "torch": torch.__version__,
        "onnx": onnx.__version__,
        "onnxruntime": ort.__version__,
        "platform": platform.platform(),
        "providers": session.get_providers(),
        "reference_output": reference.tolist(),
        "onnx_output": actual.tolist(),
        "max_abs_error": max_abs_error,
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
