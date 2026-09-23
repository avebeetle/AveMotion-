#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CORPUS = sorted((ROOT / "tests" / "corpus").glob("*.json"))


def run(command: list[str], log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.run(
            command,
            cwd=ROOT,
            stdout=stream,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    if process.returncode != 0:
        print(f"Command failed ({process.returncode}): {' '.join(command)}", file=sys.stderr)
        print(f"See {log}", file=sys.stderr)
        raise SystemExit(process.returncode)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", choices=("clang", "gcc"), default="clang")
    parser.add_argument("--output", type=Path, default=ROOT / "docs" / "characterization")
    args = parser.parse_args()

    presets = {
        ("samsung", "clang"): "linux-clang-samsung-debug",
        ("telegram", "clang"): "linux-clang-telegram-debug",
        ("samsung", "gcc"): "linux-gcc-samsung-release",
        ("telegram", "gcc"): "linux-gcc-telegram-release",
    }
    manifests: dict[str, Path] = {}
    scene_manifests: dict[str, Path] = {}
    plan_manifests: dict[str, Path] = {}
    model_manifest: Path | None = None
    parsed_model_manifest: Path | None = None
    for variant in ("samsung", "telegram"):
        preset = presets[(variant, args.compiler)]
        run(["cmake", "--preset", preset], args.output / f"{preset}-configure.log")
        run(["cmake", "--build", "--preset", preset, "-j", "2"],
            args.output / f"{preset}-build.log")
        run(["ctest", "--preset", preset], args.output / f"{preset}-ctest.log")

        executable = ROOT / "out" / "build" / preset / "avemotion_characterize"
        output = args.output / f"{variant}-{args.compiler}"
        if output.exists():
            shutil.rmtree(output)
        command = [str(executable), "--output", str(output), "--size", "128"]
        command.extend(str(path) for path in CORPUS)
        run(command, args.output / f"{preset}-characterize.log")
        manifests[variant] = output / "manifest.tsv"

        scene_executable = ROOT / "out" / "build" / preset / "avemotion_scene_characterize"
        scene_output = args.output / f"scene-{variant}-{args.compiler}"
        if scene_output.exists():
            shutil.rmtree(scene_output)
        scene_command = [str(scene_executable), "--output", str(scene_output), "--size", "128"]
        scene_command.extend(str(path) for path in CORPUS)
        run(scene_command, args.output / f"{preset}-scene-characterize.log")
        scene_manifests[variant] = scene_output / "scene_manifest.tsv"

        plan_executable = ROOT / "out" / "build" / preset / "avemotion_plan_characterize"
        plan_output = args.output / f"plan-{variant}-{args.compiler}"
        if plan_output.exists():
            shutil.rmtree(plan_output)
        plan_command = [str(plan_executable), "--output", str(plan_output), "--size", "128"]
        plan_command.extend(str(path) for path in CORPUS)
        run(plan_command, args.output / f"{preset}-plan-characterize.log")
        plan_manifests[variant] = plan_output / "plan_manifest.tsv"

        if variant == "telegram":
            model_executable = ROOT / "out" / "build" / preset / "avemotion_model_characterize"
            model_output = args.output / f"model-{variant}-{args.compiler}"
            if model_output.exists():
                shutil.rmtree(model_output)
            model_command = [str(model_executable), "--output", str(model_output)]
            model_command.extend(str(path) for path in CORPUS)
            run(model_command, args.output / f"{preset}-model-characterize.log")
            model_manifest = model_output / "model_manifest.tsv"
            parsed_model_manifest = model_output / "parsed_model_details.tsv"

    def portable(path: Path) -> str:
        try:
            return str(path.resolve().relative_to(ROOT.resolve()))
        except ValueError:
            return str(path)

    comparison = args.output / f"comparison-{args.compiler}.md"
    subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "compare_manifests.py"),
         portable(manifests["samsung"]), portable(manifests["telegram"]),
         "--output", str(comparison)],
        cwd=ROOT,
        check=True,
    )
    scene_comparison = args.output / f"scene-comparison-{args.compiler}.md"
    subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "compare_scene_manifests.py"),
         portable(scene_manifests["samsung"]), portable(scene_manifests["telegram"]),
         "--output", str(scene_comparison)],
        cwd=ROOT,
        check=True,
    )
    plan_comparison = args.output / f"plan-comparison-{args.compiler}.md"
    subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "compare_plan_manifests.py"),
         portable(plan_manifests["samsung"]), portable(plan_manifests["telegram"]),
         "--output", str(plan_comparison)],
        cwd=ROOT,
        check=True,
    )
    print(f"Reference matrix complete: {comparison}")
    print(f"Evaluated-scene matrix complete: {scene_comparison}")
    print(f"Render-plan matrix complete: {plan_comparison}")
    if model_manifest is not None:
        print(f"Immutable asset-model manifest complete: {model_manifest}")
    if parsed_model_manifest is not None:
        print(f"Detailed parsed-model manifest complete: {parsed_model_manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
