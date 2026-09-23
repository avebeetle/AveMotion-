# Part 21 validation

Final development-tree results before packaging:

```text
Telegram + Clang 17 Debug:   40/40 PASS
Telegram + GCC 14 Release:   40/40 PASS
Telegram + Clang 17 ASan:    40/40 PASS
Samsung + GCC 14 Release:    28/28 PASS
Standalone/offline Clang:    18/18 PASS
CMake install/export 0.21.0: PASS
External find_package:       PASS
```

The player test runs against both Telegram and Samsung runtime variants. The
Telegram implementation remains the product semantic baseline; this second run
only proves the scheduler is not accidentally coupled to a vendor-specific
public type.

The production Win32 preview source is statically checked to consume
`AveMotion::Player` and to contain none of the old fixed-rate scheduler symbols.
The native Part 21 MSVC/WARP gate is provided by
`scripts/run_part21_windows.cmd` and must be executed on a Windows SDK host.
