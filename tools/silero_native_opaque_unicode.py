"""Fail-closed regression checks for opaque Unicode spans in the native runtime."""

from __future__ import annotations

import argparse
import subprocess
import sys


CASES = (
    ("spaces", "\u041c\u0430\u043c\u0430 \U0001f642 \u043c\u044b\u043b\u0430 \u0440\u0430\u043c\u0443.",
     "\u041c+\u0430\u043c\u0430 \U0001f642 \u043c+\u044b\u043b\u0430 \u0440+\u0430\u043c\u0443."),
    ("attached_suffix", "\u041c\u0430\u043c\u0430\U0001f642", "\u041c+\u0430\u043c\u0430\U0001f642"),
    ("homograph_suffix", "\u042d\u0442\u043e \u0431\u043e\u043b\u044c\u0448\u043e\u0435 \u0441\u0435\u043b\u043e\U0001f642.",
     "+\u042d\u0442\u043e \u0431\u043e\u043b\u044c\u0448+\u043e\u0435 \u0441\u0435\u043b+\u043e\U0001f642."),
    ("attached_prefix", "\U0001f642\u0441\u0435\u043b\u043e", "\U0001f642\u0441\u0435\u043b+\u043e"),
    ("variation_selector", "\u0451\u043b\u043a\u0430\u2764\ufe0f", "+\u0451\u043b\u043a\u0430\u2764\ufe0f"),
    ("skin_tone", "\U0001f44d\U0001f3fd \u043c\u0430\u043c\u0430", "\U0001f44d\U0001f3fd \u043c+\u0430\u043c\u0430"),
    ("zwj_sequence", "\u0441\u0435\u043c\u044c\u044f \U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 \u0434\u043e\u043c\u0430",
     "\u0441\u0435\u043c\u044c+\u044f \U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 \u0434+\u043e\u043c\u0430"),
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
    for (case_id, _sentence, expected), output in zip(CASES, outputs):
        if output.encode("utf-8") != expected.encode("utf-8"):
            raise SystemExit(f"exact output mismatch for {case_id}: {output!r} != {expected!r}")
    print(f"opaque Unicode parity passed: cases={len(CASES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
