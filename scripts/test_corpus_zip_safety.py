#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import zipfile


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"AveMotion corpus ZIP safety test failed: {message}")


def load_runner(root: Path):
    path = root / "scripts/run_part24_corpus_lab.py"
    spec = importlib.util.spec_from_file_location("avemotion_part24_runner", path)
    require(spec is not None and spec.loader is not None, "cannot load runner")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def rejected(module, archive: Path, destination: Path) -> bool:
    try:
        module.safe_extract(archive, destination)
    except SystemExit:
        return True
    return False


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    runner = load_runner(root)
    with tempfile.TemporaryDirectory(prefix="avemotion-zip-test-") as temp:
        directory = Path(temp)
        safe_zip = directory / "safe.zip"
        with zipfile.ZipFile(safe_zip, "w") as archive:
            archive.writestr("pack/a.tgs", b"test")
        output = directory / "safe"
        runner.safe_extract(safe_zip, output)
        require((output / "pack/a.tgs").read_bytes() == b"test",
                "safe archive was not extracted")

        traversal_zip = directory / "traversal.zip"
        with zipfile.ZipFile(traversal_zip, "w") as archive:
            archive.writestr("../escape.tgs", b"bad")
        require(rejected(runner, traversal_zip, directory / "traversal"),
                "path traversal was accepted")

        absolute_zip = directory / "absolute.zip"
        with zipfile.ZipFile(absolute_zip, "w") as archive:
            archive.writestr("/absolute.tgs", b"bad")
        require(rejected(runner, absolute_zip, directory / "absolute"),
                "absolute path was accepted")
    print("AveMotion corpus ZIP safety verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
