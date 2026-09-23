#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


PARENT_CMAKE = """\
cmake_minimum_required(VERSION 3.25)
project(AveMotionCompatibility LANGUAGES C CXX)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()
if(POLICY CMP0194)
    set(CMAKE_POLICY_DEFAULT_CMP0194 OLD)
endif()
add_subdirectory(legacy)
"""

CHILD_CMAKE = """\
cmake_minimum_required(VERSION 3.3)
project(LegacyDependency LANGUAGES C CXX ASM)
add_library(legacy_dependency INTERFACE)
"""


def main() -> int:
    cmake = shutil.which("cmake")
    if not cmake:
        print("cmake executable not found", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="avemotion-cmake-compat-") as temp:
        root = Path(temp)
        child = root / "legacy"
        child.mkdir()
        (root / "CMakeLists.txt").write_text(PARENT_CMAKE, encoding="utf-8")
        (child / "CMakeLists.txt").write_text(CHILD_CMAKE, encoding="utf-8")

        command = [cmake, "-S", str(root), "-B", str(root / "build")]
        environment = dict(os.environ)
        environment.setdefault("CMAKE_GENERATOR", "Ninja")
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=environment,
            check=False,
        )
        if result.returncode != 0:
            print(result.stdout, file=sys.stderr)
            return result.returncode

    print("AveMotion legacy CMake subproject compatibility test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
