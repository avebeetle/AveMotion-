#!/usr/bin/env python3
from __future__ import annotations
import argparse
import csv
from pathlib import Path

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"AveMotion corpus lab output test failed: {message}")

def rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-assets", required=True, type=int)
    args = parser.parse_args()
    for name in ("corpus_manifest.tsv", "corpus_issues.tsv",
                 "corpus_features.tsv", "corpus_benchmarks.tsv",
                 "corpus_decision.tsv", "corpus_summary.txt"):
        require((args.output / name).is_file(), f"missing {name}")
    manifest = rows(args.output / "corpus_manifest.tsv")
    require(len(manifest) == args.expected_assets, "asset row count mismatch")
    require(all(row["asset"].startswith("asset-") for row in manifest),
            "privacy alias missing")
    require(all(row["source"] == "redacted" for row in manifest),
            "source names leaked")
    require(all(row["status"] == "ok" for row in manifest),
            "deterministic corpus has failed assets")
    decisions = rows(args.output / "corpus_decision.tsv")
    require(decisions, "decision report is empty")
    ranks = [int(row["rank"]) for row in decisions]
    require(ranks == list(range(1, len(ranks) + 1)), "ranks are not dense")
    scores = [int(row["score"]) for row in decisions]
    require(scores == sorted(scores, reverse=True), "scores are not sorted")
    benchmarks = rows(args.output / "corpus_benchmarks.tsv")
    require(len(benchmarks) == args.expected_assets, "benchmark row mismatch")
    require(all(int(row["model_bytes"]) > 0 for row in benchmarks),
            "model estimate missing")
    require(all(int(row["per_instance_bytes"]) > 0 for row in benchmarks),
            "instance estimate missing")
    summary = (args.output / "corpus_summary.txt").read_text(encoding="utf-8")
    require(f"files={args.expected_assets}" in summary, "summary count mismatch")
    require("sourceAssetsCopied=0" in summary, "privacy statement missing")
    require("rankingStatus=preliminary-until-representative-private-corpus" in summary,
            "preliminary warning missing")
    require("topCandidate=" in summary, "top candidate missing")
    copied = list(args.output.rglob("*.tgs")) + list(args.output.rglob("*.json"))
    require(not copied, "source assets were copied")
    print("AveMotion corpus lab output verification passed")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
