# Part 20.2 validation

Validated in the Linux build environment before delivery:

```text
linux-clang-telegram-debug: 36/36 CTest tests passed
linux-clang-offline:        15/15 CTest tests passed
manifest wiring guard:      passed
vendor fingerprints:        passed
CMake install/export:       passed
external find_package 0.20.2 consumer: configured, built and ran
```

A cross-generated Windows/Ninja probe confirmed that listing a `.manifest`
directly in `add_executable()` populates the generated `MANIFESTS` variable and
routes the file through `cmake -E vs_link_exe --manifests`, rather than through
a compiled `RT_MANIFEST` resource.

The real MSVC link and Win32 preview self-test must be rerun on Windows. The
change is intentionally limited to manifest wiring and does not modify runtime
or rendering behavior.
