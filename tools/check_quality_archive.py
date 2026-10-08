"""Verify the immutable v0.2.0 quality inputs remain intact."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / "tests" / "quality" / "archive" / "v0.2.0"


def main() -> int:
    corpus = (ARCHIVE / "en_ru_sentences.tsv").read_text(encoding="utf-8").splitlines()
    snapshot = (ARCHIVE / "en_ru_snapshot.tsv").read_text(encoding="utf-8").splitlines()
    if len(corpus) != 353 or corpus[0].split("\t", 1)[0] != "id":
        raise SystemExit(f"release corpus must contain 352 cases, got {len(corpus) - 1}")
    expected = {
        "cases\t352",
        "outcome\tcorrect_transformation\t278",
        "outcome\tjustified_preservation\t23",
        "outcome\tunnecessary_refusal\t34",
        "outcome\tincorrect_or_partial\t17",
    }
    if not expected.issubset(snapshot):
        raise SystemExit("release snapshot counts do not match v0.2.0 baseline")
    forbidden = ("negative-zero", "positive-sign", "negative-time", "unicode-minus")
    if any(any(token in line for token in forbidden) for line in snapshot):
        raise SystemExit("release snapshot contains post-release corrective IDs")
    print("v0.2.0 quality archive: 352 cases and baseline counts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
