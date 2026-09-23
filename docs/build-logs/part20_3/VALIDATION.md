# Part 20.3 validation

Validated before delivery in the Linux build environment:

```text
linux-clang-telegram-debug: 38/38 passed
linux-gcc-telegram-release: 38/38 passed
linux-clang-telegram-asan:  38/38 passed in deterministic groups (23 + 15)
linux-clang-offline:        17/17 passed
CMake install/export:       passed
find_package 0.20.3:        passed
external consumer:          compiled, linked and ran
```

Strict Linux characterization remained unchanged:

```text
scene golden:          exact pass
plan golden:           exact pass
source-geometry golden exact pass
```

New policy tests:

```text
avemotion.golden.cross_platform_policy:  passed
avemotion.golden.source_geometry_policy: passed
```

The MSVC branch is covered by synthetic rows containing the exact hashes from
the user's Part 20.2 Windows run. A real Windows rerun remains the final gate.
