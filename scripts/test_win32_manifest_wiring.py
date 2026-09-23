#!/usr/bin/env python3
"""Guard the Win32 preview's single-manifest CMake wiring.

CMake's MSVC/Ninja linker wrapper always generates the normal executable
manifest.  A previous preview target also compiled ``1 RT_MANIFEST`` from an
RC file, so CVTRES saw two resources with type MANIFEST, id 1 and language
0x0409 and failed with CVT1100/LNK1123.

The supported configuration is to list the custom ``.manifest`` file directly
as a target source.  CMake then passes it through ``vs_link_exe --manifests``
and performs one merge/embed operation.
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE = ROOT / "CMakeLists.txt"
PREVIEW_DIR = ROOT / "apps" / "win32_preview"
MANIFEST = PREVIEW_DIR / "AveMotionPreview.manifest"
LEGACY_RC = PREVIEW_DIR / "AveMotionPreview.rc"
MAIN_CPP = PREVIEW_DIR / "main.cpp"


def fail(message: str) -> None:
    print(f"Win32 manifest wiring verification failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def preview_target_block(cmake_text: str) -> str:
    match = re.search(
        r"add_executable\(avemotion_win32_preview\s+WIN32(?P<body>.*?)\)",
        cmake_text,
        flags=re.DOTALL,
    )
    if match is None:
        fail("avemotion_win32_preview add_executable block was not found")
    return match.group("body")


def validate_source_wiring() -> None:
    cmake_text = CMAKE.read_text(encoding="utf-8")
    block = preview_target_block(cmake_text)

    manifest_reference = "apps/win32_preview/AveMotionPreview.manifest"
    if block.count(manifest_reference) != 1:
        fail("the preview target must list AveMotionPreview.manifest exactly once")

    # Ignore CMake comments when checking source tokens. The target comment
    # intentionally explains why a legacy .rc manifest must not be restored.
    code_only = "\n".join(line.split("#", 1)[0] for line in block.splitlines())
    if re.search(r"(?i)(?:^|[\s\(])[^\s\)]*\.rc(?:$|[\s\)])", code_only):
        fail("the preview target must not compile a second manifest through an RC file")

    if LEGACY_RC.exists():
        contents = LEGACY_RC.read_text(encoding="utf-8", errors="replace")
        if "RT_MANIFEST" in contents:
            fail("legacy AveMotionPreview.rc still embeds RT_MANIFEST id 1")
        fail("legacy AveMotionPreview.rc should be removed rather than left unused")


def validate_manifest_document() -> None:
    if not MANIFEST.is_file():
        fail("AveMotionPreview.manifest is missing")

    try:
        root = ET.parse(MANIFEST).getroot()
    except ET.ParseError as error:
        fail(f"manifest XML is invalid: {error}")

    assembly_namespace = "urn:schemas-microsoft-com:asm.v1"
    if root.tag != f"{{{assembly_namespace}}}assembly":
        fail("manifest root is not an asm.v1 assembly")

    identities = root.findall(f"{{{assembly_namespace}}}assemblyIdentity")
    if len(identities) != 1:
        fail("manifest must contain exactly one assemblyIdentity")

    xml_text = MANIFEST.read_text(encoding="utf-8")
    required_fragments = {
        "PerMonitorV2": "Per-Monitor-V2 DPI awareness",
        "<longPathAware": "long-path awareness",
        ">true</longPathAware>": "enabled long-path awareness",
        "<activeCodePage": "UTF-8 active code page declaration",
        ">UTF-8</activeCodePage>": "UTF-8 active code page value",
    }
    for fragment, description in required_fragments.items():
        if fragment not in xml_text:
            fail(f"manifest is missing {description}")

    main_text = MAIN_CPP.read_text(encoding="utf-8")
    if "SetProcessDpiAwarenessContext" not in main_text:
        fail("runtime DPI fallback was removed from the Win32 preview")


def validate_generated_ninja(build_dir: Path | None) -> None:
    if build_dir is None:
        return
    ninja = build_dir / "build.ninja"
    if not ninja.is_file():
        # Visual Studio and other generators do not produce build.ninja.  The
        # source-level checks above still protect their target definition.
        return

    text = ninja.read_text(encoding="utf-8", errors="replace")
    target_lines = [
        line
        for line in text.splitlines()
        if "avemotion_win32_preview" in line or "AveMotionPreview" in line
    ]
    target_text = "\n".join(target_lines)

    # A non-Windows build does not instantiate the preview target.
    if "avemotion_win32_preview" not in target_text:
        return

    if "AveMotionPreview.rc.res" in text or "AveMotionPreview.rc" in text:
        fail("generated Ninja graph still compiles the legacy RC manifest")

    if "AveMotionPreview.manifest" not in text:
        fail("generated Ninja graph does not pass the custom manifest to CMake")

    manifest_assignment = re.search(
        r"^\s*MANIFESTS\s*=.*AveMotionPreview\.manifest.*$",
        text,
        flags=re.MULTILINE | re.IGNORECASE,
    )
    if manifest_assignment is None:
        fail("generated Ninja graph does not list the preview manifest in MANIFESTS")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    validate_source_wiring()
    validate_manifest_document()
    validate_generated_ninja(args.build_dir)
    print("AveMotion Win32 manifest wiring verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
