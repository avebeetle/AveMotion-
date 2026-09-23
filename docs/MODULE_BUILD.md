# Lean module builds and offline package

AveMotion has two deliberately different product surfaces. The Telegram module is an internal build-tree integration with the selected pinned Telegram rlottie implementation. The offline package is installable and exports only AveMotion-owned product libraries and headers, but cannot load Lottie JSON/TGS or evaluate an animation through Runtime without a reference engine. Neither surface is one DLL or a redistribution/license approval.

## Internal Telegram module

On Windows with the Visual Studio developer environment active:

```powershell
cmake --preset windows-msvc-module-release
cmake --build --preset windows-msvc-module-release --parallel 4
python scripts/test_module_build_boundaries.py --build-dir out/build/windows-msvc-module-release --variant telegram --direct2d yes
```

On Linux with GCC:

```sh
cmake --preset linux-gcc-module-release
cmake --build --preset linux-gcc-module-release --parallel 4
python3 scripts/test_module_build_boundaries.py --build-dir out/build/linux-gcc-module-release --variant telegram --direct2d no
```

These opt-in Release presets disable testing, validation/characterization tools, the private corpus lab, preview and capture executables. Windows includes Direct2D; Linux does not. The build produces separate product static archives plus the selected Telegram upstream archive. It is a build-tree surface; `AVEMOTION_ENABLE_INSTALL=ON` still fails for a reference-linked configuration.

## Offline product package

On Windows:

```powershell
cmake --preset windows-msvc-offline-package
cmake --build --preset windows-msvc-offline-package --parallel 4
cmake --install out/build/windows-msvc-offline-package --prefix C:/work/avemotion-offline
python scripts/test_installed_package_boundaries.py --prefix C:/work/avemotion-offline --direct2d yes --self-test
cmake -S tests/consumer -B out/consumer-offline -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/work/avemotion-offline
cmake --build out/consumer-offline
out/consumer-offline/avemotion_installed_consumer.exe
```

On Linux with Clang:

```sh
cmake --preset linux-clang-offline-package
cmake --build --preset linux-clang-offline-package --parallel 4
cmake --install out/build/linux-clang-offline-package --prefix /tmp/avemotion-offline
python3 scripts/test_module_build_boundaries.py --build-dir out/build/linux-clang-offline-package --variant none --direct2d no
python3 scripts/test_installed_package_boundaries.py --prefix /tmp/avemotion-offline --direct2d no --self-test
cmake -S tests/consumer -B out/consumer-offline -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/tmp/avemotion-offline
cmake --build out/consumer-offline
out/consumer-offline/avemotion_installed_consumer
```

Use a fresh prefix for each install so stale files cannot masquerade as package contents. The package exports `AveMotion::Formats`, `Core`, `Model`, `Validation`, `Evaluation`, `Runtime`, `Player`, `Rendering`, and on Windows `Direct2D`. These remain separate static libraries. The public Runtime loader returns `RuntimeErrorCode::ReferenceUnavailable` for valid Lottie JSON in this no-reference package; the consumer checks that behavior. This is useful for product API/packaging integration, not successful animation playback.

`AveMotion::Reference` and `include/avemotion/reference/` remain available in source and build-tree laboratory configurations. Experimental consumers of that facade should use the source/build-tree target, not the installed package. The development presets and their full tests are unchanged. Preserve vendor licenses/notices and obtain a separate redistribution decision before shipping a Telegram-backed runtime. No performance, race-detector or stable-ABI claim follows from these build profiles.

## Vendor integrity and comparison CI

`python scripts/verify_vendor.py` checks both retained Telegram and Samsung source trees plus the committed corpus by default. Use `--variant telegram` for the internal module, `--variant none` for the offline package's corpus-only check, or `--variant samsung` to inspect that tree alone. The Samsung comparison build uses `--variant all` through CTest, so it checks both trees. Every selection still checks the committed corpus hashes. CTest also runs the independent fingerprint-ordering, selection, and protected-byte regressions in ordinary testing configurations.

Normal push and pull request CI keeps Telegram Debug, Release, ASan, no-reference, Direct2D WARP, capture and preview checks, and adds lean build/package boundaries on Linux and Windows. `.github/workflows/samsung-comparison.yml` runs only by manual workflow dispatch and retains Samsung Linux Debug, GCC Release, ASan and Windows Debug with full tests and diagnostic uploads. Samsung golden failures remain visible when the optional comparison runs; this workflow does not relax or filter them.
