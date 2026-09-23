#!/usr/bin/env python3
"""Behavior regression: real standalone reports, complete profile corruptions."""
from __future__ import annotations
import argparse
import csv
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def read(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def write(path, rows):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    args = parser.parse_args()
    validator = Path(__file__).with_name("test_corpus_lab_output.py")
    checks = 0
    with tempfile.TemporaryDirectory(prefix="avemotion-lifetime-") as temporary:
        root = Path(temporary)
        real = root / "real"
        subprocess.run([str(args.executable), "--input", str(args.corpus), "--output", str(real),
                        "--samples", "3", "--warmup-samples", "2", "--load-repeats", "1",
                        "--cpu-repeats", "0", "--render-size", "64", "--strict"], check=True)
        manifest = read(real / "corpus_manifest.tsv")
        frames = {row["asset"]: int(row["frames"]) for row in manifest}

        def validate(directory, extra, success, label):
            nonlocal checks
            result = subprocess.run([sys.executable, str(validator), "--output", str(directory),
                "--expected-assets", str(len(manifest)), "--expected-samples", "3",
                "--expected-warmup-samples", "2", *extra], capture_output=True, text=True)
            if (result.returncode == 0) != success:
                raise AssertionError(f"{label}: exit={result.returncode}\n{result.stdout}{result.stderr}")
            checks += 1

        fields = [f"{phase}_{role}_sessions" for phase in ("setup", "first", "steady")
                  for role in ("metadata", "scene", "model", "cpu")]
        fields += ["setup_model_samples", "first_scene_samples", "steady_scene_samples"]
        for profile, setup in (("persistent", 1), ("fresh", 0), ("fresh", 1)):
            target = root / f"{profile}-{setup}"
            shutil.copytree(real, target)
            rows = read(target / "corpus_benchmarks.tsv")
            for row in rows:
                for field in fields:
                    row[field] = "0"
                row.update(setup_metadata_sessions="1", setup_scene_sessions=str(setup),
                           setup_model_sessions=str(1 if profile == "persistent" else frames[row["asset"]]),
                           setup_model_samples=str(frames[row["asset"]]), first_scene_samples="1",
                           steady_scene_samples="3", first_scene_sessions="0" if profile == "persistent" else "1",
                           steady_scene_sessions="0" if profile == "persistent" else "3")
            write(target / "corpus_benchmarks.tsv", rows)
            # Persistent default is tested without specifying --lifetime-profile.
            extra = ([] if profile == "persistent" else ["--lifetime-profile", profile])
            extra += ["--expected-setup-scene-sessions", str(setup)]
            validate(target, extra, True, f"valid {profile}/{setup}")
            if profile == "fresh" and setup == 0:
                validate(target, ["--lifetime-profile", "fresh"], True, "fresh default setup")
            for field in fields:
                modified = [dict(row) for row in rows]
                modified[0][field] = str(int(modified[0][field]) + 1)
                write(target / "corpus_benchmarks.tsv", modified)
                validate(target, extra, False, f"reject {profile} corrupt {field}")
                for row in modified:
                    del row[field]
                write(target / "corpus_benchmarks.tsv", modified)
                validate(target, extra, False, f"reject {profile} missing {field}")
            write(target / "corpus_benchmarks.tsv", rows)
            opposite = "fresh" if profile == "persistent" else "persistent"
            validate(target, ["--lifetime-profile", opposite, "--expected-setup-scene-sessions", str(setup)],
                     False, f"reject wrong {opposite} profile")
            changed_manifest = [dict(row) for row in manifest]
            changed_manifest[0]["frames"] = str(int(changed_manifest[0]["frames"]) + 1)
            write(target / "corpus_manifest.tsv", changed_manifest)
            validate(target, extra, False, "reject manifest frame mismatch")
        # The real executable itself must emit the persistent default contract.
        validate(real, [], True, "real persistent report")
    print(f"Corpus lifetime profile behavior passed: {checks} positive/rejection checks")


if __name__ == "__main__":
    main()
