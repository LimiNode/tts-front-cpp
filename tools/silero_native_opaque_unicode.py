"""Fail-closed regression checks for opaque Unicode spans in the native runtime."""

from __future__ import annotations

import argparse
import subprocess
import sys


CASES = (
    ("spaces", "Мама 🙂 мыла раму.", ("🙂",)),
    ("attached_suffix", "Мама🙂", ("🙂",)),
    ("homograph_suffix", "Это большое село🙂.", ("🙂",)),
    ("attached_prefix", "🙂село", ("🙂",)),
    ("variation_selector", "ёлка❤️", ("❤️",)),
    ("skin_tone", "👍🏽 мама", ("👍🏽",)),
    ("zwj_sequence", "семья 👨‍👩‍👧‍👦 дома", ("👨‍👩‍👧‍👦",)),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--assets", required=True)
    parser.add_argument("--stress", required=True)
    parser.add_argument("--yo", required=True)
    parser.add_argument("--homo", required=True)
    args = parser.parse_args()

    command = [
        args.executable,
        "--assets",
        args.assets,
        "--stress",
        args.stress,
        "--yo",
        args.yo,
        "--homo",
        args.homo,
    ]
    payload = "".join(sentence + "\n" for _, sentence, _ in CASES).encode("utf-8")
    completed = subprocess.run(command, input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        sys.stderr.buffer.write(completed.stderr)
        raise SystemExit(f"native opaque Unicode regression failed: exit {completed.returncode}")

    outputs = completed.stdout.decode("utf-8").splitlines()
    if len(outputs) != len(CASES):
        raise SystemExit(f"expected {len(CASES)} outputs, got {len(outputs)}")
    for (case_id, _sentence, opaque), output in zip(CASES, outputs):
        for span in opaque:
            if span not in output:
                raise SystemExit(f"opaque span lost for {case_id}: {span!r}")
    print(f"opaque Unicode parity passed: cases={len(CASES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
