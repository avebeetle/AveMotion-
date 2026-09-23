#!/usr/bin/env python3
"""Verify that an installed AveMotion prefix exports only product capabilities."""
from __future__ import annotations

import argparse
import re
import shutil
import sys
import tempfile
from pathlib import Path

TARGETS = {"Formats", "Core", "Model", "Validation", "Evaluation", "Runtime", "Player", "Rendering"}
ARCHIVES = {"avemotion_formats", "avemotion_core", "avemotion_model", "avemotion_validation",
            "avemotion_evaluation", "avemotion_runtime", "avemotion_player", "avemotion_rendering"}
HEADERS = {"formats/Tgs.hpp", "core/Hash.hpp", "model/AssetModel.hpp",
           "validation/AssetValidator.hpp", "evaluation/PropertyEvaluator.hpp",
           "runtime/Runtime.hpp", "player/Player.hpp", "render/RenderPlan.hpp"}


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def archive_stem(path: Path) -> str | None:
    name = path.name.lower()
    if name.endswith(".lib"):
        return name[:-4]
    if name.startswith("lib") and name.endswith(".a"):
        return name[3:-2]
    return None


def check(prefix: Path, direct2d: bool) -> None:
    prefix = prefix.resolve(strict=True)
    include = prefix / "include/avemotion"
    lib_dirs = [prefix / name for name in ("lib", "lib64") if (prefix / name).is_dir()]
    require(include.is_dir() and lib_dirs, "missing product include/lib layout")
    require(not (include / "reference").exists(), "Reference header subtree installed")
    expected_headers = set(HEADERS)
    expected_targets = set(TARGETS)
    expected_archives = set(ARCHIVES)
    if direct2d:
        expected_headers.add("backends/direct2d/Direct2DBackend.hpp")
        expected_targets.add("Direct2D")
        expected_archives.add("avemotion_backend_d2d")
    for name in expected_headers:
        require((include / name).is_file(), f"missing product header: {name}")

    cmake_dirs = [path / "cmake/AveMotion" for path in lib_dirs]
    cmake_dirs = [path for path in cmake_dirs if path.is_dir()]
    require(len(cmake_dirs) == 1, "missing or ambiguous AveMotion CMake package")
    cmake_dir = cmake_dirs[0]
    for name in ("AveMotionConfig.cmake", "AveMotionConfigVersion.cmake", "AveMotionTargets.cmake"):
        require((cmake_dir / name).is_file(), f"missing CMake package file: {name}")
    export = (cmake_dir / "AveMotionTargets.cmake").read_text(encoding="utf-8")
    declarations = set(re.findall(r"^add_library\(AveMotion::([A-Za-z0-9_]+)\s+STATIC\s+IMPORTED\)",
                                  export, re.MULTILINE))
    require(declarations == expected_targets,
            f"product import declarations mismatch: missing={sorted(expected_targets-declarations)}, "
            f"extra={sorted(declarations-expected_targets)}")

    files = [path for path in prefix.rglob("*") if path.is_file()]
    require(not any("reference" in part.lower() for path in files
                    for part in path.relative_to(prefix).parts), "Reference file/header/archive installed")
    require(not any("rlottie" in path.name.lower() or "samsung" in path.name.lower()
                    for path in files), "upstream payload installed")
    require(not any(path.parent.name.lower() == "bin" or path.suffix.lower() == ".exe"
                    for path in files), "executable installed")
    archive_paths = [(stem, path) for path in files
                     for stem in (archive_stem(path),) if stem is not None]
    actual_archives = {stem for stem, _ in archive_paths}
    require(actual_archives == expected_archives,
            f"product archives mismatch: missing={sorted(expected_archives-actual_archives)}, "
            f"extra={sorted(actual_archives-expected_archives)}")
    require(len(archive_paths) == len(expected_archives) and
            all(path.parent in lib_dirs for _, path in archive_paths),
            "product archive outside library layout or duplicated")
    product_header_roots = {"backends", "core", "evaluation", "formats", "model",
                            "player", "render", "runtime", "validation"}
    for path in files:
        relative = path.relative_to(prefix).as_posix().lower()
        require(not any(word in relative for word in ("corpus", "characterize", "preview", "probe", "_tests")),
                f"laboratory/test payload installed: {relative}")
        if path.is_relative_to(include):
            parts = path.relative_to(include).parts
            allowed_layout = (len(parts) >= 2 and parts[0] in product_header_roots
                              and path.suffix.lower() in (".h", ".hpp"))
        elif path.parent in lib_dirs:
            allowed_layout = archive_stem(path) in expected_archives
        elif path.parent == cmake_dir:
            allowed_layout = (path.name in ("AveMotionConfig.cmake", "AveMotionConfigVersion.cmake",
                                            "AveMotionTargets.cmake") or
                              (path.name.startswith("AveMotionTargets-") and path.suffix == ".cmake"))
        else:
            allowed_layout = False
        require(allowed_layout, f"unexpected installed payload outside product layout: {relative}")
    for path in cmake_dir.glob("*.cmake"):
        content = path.read_text(encoding="utf-8")
        active = "\n".join(line for line in content.splitlines()
                           if not line.lstrip().startswith("#"))
        require(re.search(r'(?:^|["(;=\s])(?:[A-Za-z]:[/\\]|/(?!["\s)]))',
                          active, re.MULTILINE) is None,
                f"absolute source/build path leaked in {path.name}")
        for quoted in re.findall(r'"([^"]*)"', content):
            for piece in quoted.split(";"):
                require(not re.match(r"^(?:[A-Za-z]:[/\\]|/(?!$))", piece),
                        f"absolute source/build path leaked in {path.name}: {piece}")
    print(f"INSTALLED_BOUNDARY_OK prefix={prefix} direct2d={'yes' if direct2d else 'no'}")


def synthetic_self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="avemotion-package-boundary-") as temp:
        root = Path(temp)
        valid = root / "valid"
        include = valid / "include/avemotion"
        lib = valid / "lib"
        cmake = lib / "cmake/AveMotion"
        cmake.mkdir(parents=True)
        for header in HEADERS:
            path = include / header
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// synthetic product header\n", encoding="utf-8")
        for archive in ARCHIVES:
            (lib / f"{archive}.lib").write_bytes(b"synthetic archive")
        for name in ("AveMotionConfig.cmake", "AveMotionConfigVersion.cmake"):
            (cmake / name).write_text("# synthetic package\n", encoding="utf-8")
        export = cmake / "AveMotionTargets.cmake"
        export.write_text("".join(f"add_library(AveMotion::{name} STATIC IMPORTED)\n"
                                  for name in sorted(TARGETS)), encoding="utf-8")
        check(valid, False)
        linux_lib64 = root / "linux_lib64"
        shutil.copytree(valid, linux_lib64)
        (linux_lib64 / "lib").rename(linux_lib64 / "lib64")
        for archive in ARCHIVES:
            (linux_lib64 / "lib64" / f"{archive}.lib").rename(
                linux_lib64 / "lib64" / f"lib{archive}.a")
        check(linux_lib64, False)
        empty_reference = root / "empty_reference"
        shutil.copytree(valid, empty_reference)
        (empty_reference / "include/avemotion/reference").mkdir()
        try:
            check(empty_reference, False)
        except ValueError:
            pass
        else:
            raise ValueError("self-test failed to reject Reference header subtree")
        misplaced = root / "misplaced_product"
        shutil.copytree(valid, misplaced)
        (misplaced / "lib/avemotion_runtime.lib").rename(misplaced / "avemotion_runtime.lib")
        try:
            check(misplaced, False)
        except ValueError:
            pass
        else:
            raise ValueError("self-test failed to reject archive outside library layout")
        mutations = {
            "reference_header": lambda p: (p / "include/avemotion/reference/ReferenceRuntime.hpp"),
            "reference_archive": lambda p: (p / "lib/avemotion_reference.lib"),
            "executable": lambda p: (p / "bin/avemotion_probe.exe"),
            "suffixless_executable": lambda p: (p / "libexec/avemotion_helper"),
            "upstream": lambda p: (p / "lib/rlottie.lib"),
        }
        for label, path_fn in mutations.items():
            copy = root / label
            shutil.copytree(valid, copy)
            path = path_fn(copy)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"forbidden")
            try:
                check(copy, False)
            except ValueError:
                pass
            else:
                raise ValueError(f"self-test failed to reject {label}")
        for label, suffix in (("reference_export", "add_library(AveMotion::Reference STATIC IMPORTED)\n"),
                              ("path_leak", 'set(LEAK "C:/source/build/Runtime.cpp")\n'),
                              ("path_leak_unquoted", "set(LEAK C:/source/build/Runtime.cpp)\n")):
            copy = root / label
            shutil.copytree(valid, copy)
            with (copy / "lib/cmake/AveMotion/AveMotionTargets.cmake").open("a", encoding="utf-8") as stream:
                stream.write(suffix)
            try:
                check(copy, False)
            except ValueError:
                pass
            else:
                raise ValueError(f"self-test failed to reject {label}")
        copy = root / "missing_product"
        shutil.copytree(valid, copy)
        (copy / "lib/avemotion_runtime.lib").unlink()
        try:
            check(copy, False)
        except ValueError:
            pass
        else:
            raise ValueError("self-test failed to reject missing product archive")
    print("INSTALLED_BOUNDARY_SELF_TEST_OK")


def real_prefix_mutations(prefix: Path, direct2d: bool) -> None:
    """Copy a verified real package for each independent negative mutation."""
    with tempfile.TemporaryDirectory(prefix="avemotion-real-prefix-mutations-") as temp:
        root = Path(temp)
        lib_name = "lib" if (prefix / "lib/cmake/AveMotion").is_dir() else "lib64"
        mutations = {
            "reference_header": ("include/avemotion/reference/ReferenceRuntime.hpp", b"forbidden"),
            "reference_archive": (f"{lib_name}/avemotion_reference.lib", b"forbidden"),
            "executable": ("bin/avemotion_probe.exe", b"forbidden"),
            "suffixless_executable": ("libexec/avemotion_helper", b"forbidden"),
            "path_leak": (f"{lib_name}/cmake/AveMotion/AveMotionTargets.cmake", b'\nset(LEAK "C:/source/build/Runtime.cpp")\n'),
        }
        for label, (relative, content) in mutations.items():
            copy = root / label
            shutil.copytree(prefix, copy)
            path = copy / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            if label == "path_leak":
                with path.open("ab") as stream:
                    stream.write(content)
            else:
                path.write_bytes(content)
            try:
                check(copy, direct2d)
            except ValueError:
                pass
            else:
                raise ValueError(f"real-prefix mutation not rejected: {label}")
        copy = root / "reference_export"
        shutil.copytree(prefix, copy)
        with (copy / lib_name / "cmake/AveMotion/AveMotionTargets.cmake").open("a", encoding="utf-8") as stream:
            stream.write("\nadd_library(AveMotion::Reference STATIC IMPORTED)\n")
        try:
            check(copy, direct2d)
        except ValueError:
            pass
        else:
            raise ValueError("real-prefix Reference export not rejected")
        copy = root / "missing_product"
        shutil.copytree(prefix, copy)
        archive = copy / lib_name / "avemotion_runtime.lib"
        if not archive.exists():
            archive = copy / lib_name / "libavemotion_runtime.a"
        archive.unlink()
        try:
            check(copy, direct2d)
        except ValueError:
            pass
        else:
            raise ValueError("real-prefix missing Runtime archive not rejected")
    print("INSTALLED_BOUNDARY_REAL_MUTATIONS_OK cases=7")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", type=Path)
    parser.add_argument("--direct2d", choices=("yes", "no"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            synthetic_self_test()
        if args.prefix is not None:
            require(args.direct2d is not None, "--direct2d is required with --prefix")
            check(args.prefix, args.direct2d == "yes")
            if args.self_test:
                real_prefix_mutations(args.prefix, args.direct2d == "yes")
        else:
            require(args.self_test, "--prefix is required without --self-test")
    except (OSError, ValueError) as error:
        print(f"INSTALLED_BOUNDARY_REJECTED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
