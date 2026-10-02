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
import subprocess
import sys
from pathlib import Path


PINNED_SOURCE_REVISION = "d38096cae9bf3ac846bbb88705428f3ed3801b96"
DEFAULT_MODEL_RELATIVE_PATH = "src/silero_stress/data/accentor.pt"

VECTORS = (
    {
        "id": "known_vocabulary",
        "category": "known_vocabulary",
        "input": "\u041c\u0430\u043c\u0430 \u043c\u044b\u043b\u0430 \u0440\u0430\u043c\u0443.",
    },
    {
        "id": "oov",
        "category": "oov",
        "input": "\u041a\u0432\u0430\u043d\u0442\u043e\u043b\u0438\u043a.",
    },
    {
        "id": "homograph_selo_subject",
        "category": "homograph_context",
        "input": "\u042d\u0442\u043e \u0441\u0435\u043b\u043e.",
    },
    {
        "id": "homograph_selo_object",
        "category": "homograph_context",
        "input": "\u042f \u0432\u0438\u0436\u0443 \u0441\u0435\u043b\u043e.",
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
        from silero_stress import load_accentor
    except ImportError as error:
        raise SystemExit("install the pinned Silero Python dependencies (torch)") from error

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    accentor = load_accentor("ru")

    records = [
        {
            "record_type": "metadata",
            "project": "snakers4/silero-stress",
            "source_revision": revision,
            "model_relative_path": str(model.relative_to(source)).replace("\\", "/")
            if model.is_relative_to(source)
            else None,
            "model_sha256": sha256(model),
            "python": platform.python_version(),
            "torch": torch.__version__,
            "platform": platform.platform(),
            "torch_num_threads": torch.get_num_threads(),
            "torch_num_interop_threads": torch.get_num_interop_threads(),
            "stress_marker": "+",
            "output_contract": "SileroStress.__call__ output with + stress markers",
        }
    ]
    for vector in VECTORS:
        output = accentor(vector["input"])
        records.append({**vector, "record_type": "vector", "output": output})

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
