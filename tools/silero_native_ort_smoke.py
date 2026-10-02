#!/usr/bin/env python3
"""Execute the native ORT probe against a tiny generated Identity graph."""

from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    try:
        import onnx
        from onnx import TensorProto, helper
    except ImportError as error:
        raise SystemExit("install onnx for the native ORT smoke test") from error

    graph = helper.make_graph(
        [helper.make_node("Identity", ["input"], ["output"])],
        "native_ort_identity_smoke",
        [helper.make_tensor_value_info("input", TensorProto.FLOAT, [2, 2])],
        [helper.make_tensor_value_info("output", TensorProto.FLOAT, [2, 2])],
    )
    model = helper.make_model(
        graph,
        opset_imports=[helper.make_opsetid("", 17)],
        producer_name="tts-front-cpp",
    )
    with tempfile.TemporaryDirectory(prefix="silero-native-ort-smoke-") as temporary:
        root = Path(temporary)
        graph_path = root / "identity.onnx"
        input_path = root / "input.txt"
        onnx.save(model, graph_path)
        input_path.write_text("2 2\n1 2 3 4\n", encoding="ascii")
        completed = subprocess.run(
            [
                str(args.probe.resolve()),
                "--graph",
                str(graph_path),
                "--kind",
                "accentor",
                "--input",
                str(input_path),
            ],
            text=True,
            encoding="utf-8",
            capture_output=True,
            check=False,
        )
    if completed.returncode != 0:
        raise SystemExit(f"native ORT smoke failed: {completed.stderr.strip()}")
    expected = "shape=2,2\nvalues=1,2,3,4\n"
    if completed.stdout != expected:
        raise SystemExit(f"unexpected native ORT smoke output: {completed.stdout!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
