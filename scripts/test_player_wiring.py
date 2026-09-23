#!/usr/bin/env python3
"""Verify that the Win32 preview consumes the portable Part 21 player."""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
HEADER = (ROOT / "apps/win32_preview/PreviewApp.hpp").read_text(encoding="utf-8")
SOURCE = (ROOT / "apps/win32_preview/PreviewApp.cpp").read_text(encoding="utf-8")

errors: list[str] = []

required_cmake = [
    "add_library(avemotion_player STATIC",
    "add_library(AveMotion::Player ALIAS avemotion_player)",
    "AveMotion::Player",
]
for value in required_cmake:
    if value not in CMAKE:
        errors.append(f"missing CMake player wiring: {value}")

required_header = [
    '#include "avemotion/player/Player.hpp"',
    "player::Player player_;",
    "playerScheduleWakeup",
    "scheduleFrameWakeup",
]
for value in required_header:
    if value not in HEADER:
        errors.append(f"missing PreviewApp.hpp player wiring: {value}")

required_source = [
    "player_.addInstance",
    "player_.tick",
    "player_.setVisible",
    "player_.play",
    "player_.pause",
    "player_.setDirection",
    "player_.setLoopMode",
    "player_.setPlaybackRate",
    "player_.seekNormalized",
    "PreviewApplication::playerScheduleWakeup",
]
for value in required_source:
    if value not in SOURCE:
        errors.append(f"missing PreviewApp.cpp player integration: {value}")

for forbidden in (
    "armFrameTimer",
    "nextFrameCounter_",
    "kTargetFrameRate",
):
    if forbidden in HEADER or forbidden in SOURCE:
        errors.append(f"legacy preview-local scheduler remains: {forbidden}")

if errors:
    print("AveMotion player wiring verification failed:")
    for error in errors:
        print(f"- {error}")
    sys.exit(1)

print("AveMotion centralized player wiring verification passed")
