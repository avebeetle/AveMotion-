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

def summary_values(path: Path) -> dict[str, str]:
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines()
                if "=" in line)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-assets", required=True, type=int)
    parser.add_argument("--expected-samples", type=int, default=5)
    parser.add_argument("--expected-warmup-samples", type=int, default=20)
    parser.add_argument("--expected-render-size", type=int, default=64)
    parser.add_argument("--expected-setup-scene-sessions", type=int, default=0)
    args = parser.parse_args()
    require(args.expected_setup_scene_sessions >= 0,
            "expected setup scene sessions must be nonnegative")
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
    require([row["asset"] for row in benchmarks] == [row["asset"] for row in manifest],
            "benchmark aliases differ from manifest aliases")
    for row in benchmarks:
        for prefix in ("exact_scene", "pipeline"):
            median = int(row[f"{prefix}_median_ns"])
            p95 = int(row[f"{prefix}_p95_ns"])
            require(0 <= median <= p95, f"invalid {prefix} percentile")
        for phase in ("setup", "first", "steady"):
            for role in ("metadata", "scene", "model", "cpu"):
                name = f"{phase}_{role}_sessions"
                require(int(row[name]) >= 0, f"negative {name}")
        for name in ("setup_model_samples", "first_scene_samples", "steady_scene_samples"):
            require(int(row[name]) >= 0, f"negative {name}")
        require(int(row["setup_metadata_sessions"]) == 1,
                "setup must include the asset metadata session")
        require(int(row["setup_scene_sessions"]) == args.expected_setup_scene_sessions,
                "setup scene session count mismatch")
        require(int(row["setup_model_samples"]) > 0,
                "setup must include model preparation samples")
        require(int(row["first_scene_samples"]) == 1,
                "first phase must contain exactly one scene sample")
        require(int(row["steady_scene_samples"]) == args.expected_samples,
                "steady phase scene samples must match measured CLI count")
        require(all(int(row[f"{phase}_cpu_sessions"]) == 0
                    for phase in ("setup", "first", "steady")),
                "CPU oracle ran inside a diagnostic phase")
        for boundary in ("after_prepare", "after_warmup", "after_measured"):
            for owner in ("evaluator", "projector"):
                for measure in ("retained_bytes", "storage_generation"):
                    name = f"{owner}_{boundary}_{measure}"
                    require(int(row[name]) >= 0, f"negative {name}")
    require(all(int(row["model_bytes"]) > 0 for row in benchmarks),
            "model estimate missing")
    require(all(int(row["per_instance_bytes"]) > 0 for row in benchmarks),
            "instance estimate missing")
    summary = (args.output / "corpus_summary.txt").read_text(encoding="utf-8")
    values = summary_values(args.output / "corpus_summary.txt")
    require(values.get("schema") == "2", "schema must be 2")
    require(values.get("samplesPerAsset") == str(args.expected_samples), "measured sample count mismatch")
    require(values.get("measuredSamplesPerAsset") == str(args.expected_samples),
            "explicit measured sample count mismatch")
    require(values.get("warmupSamplesPerAsset") == str(args.expected_warmup_samples),
            "warm-up sample count mismatch")
    require(values.get("renderSize") == str(args.expected_render_size), "viewport mismatch")
    require(values.get("viewport") == f"{args.expected_render_size}x{args.expected_render_size}",
            "explicit viewport mismatch")
    require(values.get("frameOrder") == "first=0,warmup=sample%frames,steady=sample%frames",
            "frame order mismatch")
    require(values.get("timingUnits") == "nanoseconds", "timing units mismatch")
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
