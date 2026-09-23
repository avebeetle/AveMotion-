# Part 22 validation summary

Environment:

```text
Linux x86-64
CMake 3.31.6
Ninja 1.12.1
Clang 17
GCC 14.2
Python 3.13
```

Completed configurations:

```text
linux-clang-offline             21/21 PASS
linux-clang-telegram-debug      43/43 PASS
linux-gcc-telegram-release      43/43 PASS
linux-clang-telegram-asan       43/43 PASS
linux-gcc-samsung-release       31/31 PASS
linux-clang-samsung-asan        31/31 PASS
```

Additional checks:

```text
scripts/test_tgs_wiring.py      PASS
fixture SHA-256                 PASS
CMake install/export 0.22.0     PASS
external find_package consumer PASS
```

The Samsung configuration initially exposed duplicate historical miniz symbols
because both Samsung rlottie and AveMotion compiled the same single-header
implementation. The final private wrapper prefixes all emitted miniz symbols;
Samsung Release and ASan builds then linked and passed.

Windows validation command:

```bat
scripts\run_part22_windows.cmd
```

The Windows gate was not executed in the Linux build environment.
