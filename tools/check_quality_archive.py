"""Verify the immutable v0.2.0 and v0.2.1 quality archives."""

from __future__ import annotations

from pathlib import Path
import hashlib


ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / "tests" / "quality" / "archive" / "v0.2.0"
CURRENT_ARCHIVE = ROOT / "tests" / "quality" / "archive" / "v0.2.1"


def verify_archive(
    archive: Path,
    expected_hashes: dict[str, str],
    expected_counts: set[str],
    expected_cases: int,
    label: str,
) -> None:
    corpus = (archive / "en_ru_sentences.tsv").read_text(encoding="utf-8").splitlines()
    snapshot = (archive / "en_ru_snapshot.tsv").read_text(encoding="utf-8").splitlines()
    for name, expected_hash in expected_hashes.items():
        data = (archive / name).read_bytes().replace(b"\r\n", b"\n")
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise SystemExit(f"{label} archive checksum mismatch: {name}")
    if len(corpus) != expected_cases + 1 or corpus[0].split("\t", 1)[0] != "id":
        raise SystemExit(f"{label} release corpus must contain {expected_cases} cases, got {len(corpus) - 1}")
    if not expected_counts.issubset(snapshot):
        raise SystemExit(f"{label} release snapshot counts do not match expected baseline")


def main() -> int:
    verify_archive(
        ARCHIVE,
        {
            "en_ru_sentences.tsv": "64df6a2991f4f4e936ac3a33bd96cb4cbe5fc074710e4e7d2915e029ebc42c7c",
            "en_ru_snapshot.tsv": "d17ae423a174bf57215d06878cf3791a9879bbd7d2ed4030549bc3d9adc36d0e",
        },
        {
            "cases\t352",
            "outcome\tcorrect_transformation\t278",
            "outcome\tjustified_preservation\t23",
            "outcome\tunnecessary_refusal\t34",
            "outcome\tincorrect_or_partial\t17",
        },
        352,
        "v0.2.0",
    )

    verify_archive(
        CURRENT_ARCHIVE,
        {
            "en_ru_sentences.tsv": "d4c6bcda3d9ebf1602075a6565f2735642fc96e4ef5f4bfbc081ce9416e08033",
            "en_ru_snapshot.tsv": "c0458f2b84ee41679b654bd464e4e745411fafb48f5e7465469a8b371c4eb947",
        },
        {
            "cases\t373",
            "outcome\tcorrect_transformation\t324",
            "outcome\tjustified_preservation\t34",
            "outcome\tunnecessary_refusal\t2",
            "outcome\tincorrect_or_partial\t13",
        },
        373,
        "v0.2.1",
    )

    snapshot = (ARCHIVE / "en_ru_snapshot.tsv").read_text(encoding="utf-8").splitlines()
    forbidden = ("negative-zero", "positive-sign", "negative-time", "unicode-minus")
    if any(any(token in line for token in forbidden) for line in snapshot):
        raise SystemExit("release snapshot contains post-release corrective IDs")
    print("v0.2.0 quality archive: 352 cases and baseline counts verified")
    print("v0.2.1 quality archive: 373 cases and baseline counts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
