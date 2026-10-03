#!/usr/bin/env python3
"""Exercise fail-closed rejection of incompatible native bundle identities."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    return parser.parse_args()


def clone_bundle(source: Path, destination: Path) -> None:
    destination.mkdir()
    for path in source.iterdir():
        target = destination / path.name
        if path.is_file():
            if path.name == "manifest.json":
                shutil.copy2(path, target)
                continue
            try:
                os.link(path, target)
            except OSError:
                shutil.copy2(path, target)


def main() -> int:
    args = parse_args()
    manifest_path = args.assets / "manifest.json"
    original = json.loads(manifest_path.read_text(encoding="utf-8"))
    mutations = {
        "source_revision": "0" * 40,
        "model_sha256": "0" * 64,
    }
    with tempfile.TemporaryDirectory(prefix="silero-native-negative-") as temporary:
        root = Path(temporary)
        for field, replacement in mutations.items():
            bundle = root / field
            clone_bundle(args.assets, bundle)
            manifest = dict(original)
            manifest[field] = replacement
            (bundle / "manifest.json").write_text(
                json.dumps(manifest, ensure_ascii=True, indent=2) + "\n", encoding="utf-8"
            )
            command = [
                str(args.executable),
                "--assets",
                str(bundle),
                "--stress",
                str(bundle / "stress.onnx"),
                "--yo",
                str(bundle / "yo.onnx"),
                "--homo",
                str(bundle / "homosolver.onnx"),
            ]
            completed = subprocess.run(command, input=b"", stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            if completed.returncode == 0:
                raise SystemExit(f"incompatible {field} was accepted")
            print(f"{field}: rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
