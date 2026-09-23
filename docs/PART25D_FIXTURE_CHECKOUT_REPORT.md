# Part 25D fixture checkout fidelity

Dispatch base: `447272441328c5518884bf2aa8d9a1136065845f` on `main`.

## Cause and correction

Git with `core.autocrlf=true` expanded LF to CRLF in `tests/fixtures`, which had no byte-protection attribute. The compatibility generator reads those JSON files as raw bytes, while committed TGS payloads retain the authored LF bytes. The earlier actual-checkout probe in `out/part25d-fixture-checkout/findings.md` reproduced failed corpus integrity and TGS source identity in that state. This change adds exactly `/tests/fixtures/** -text`, consistent with the existing protected corpus and compatibility families. Runtime parsing, hashes, fixture contents, payloads, manifests, goldens, and vendor content were not changed.

## Regression evidence

The existing `CASES` roundtrip mapping gained four literal fixture cases: root JSON, nested reference-session JSON, TGS `SHA256SUMS.txt`, and binary TGS. The production change that makes this test fail is removal of fixture byte protection. With the old attributes, `python scripts/test_git_protected_bytes.py --git git --attributes .gitattributes` exited 1: the three fixture text probes acquired `\r\n` under `core.autocrlf=true`; all previously protected and binary cases matched. The command did not fail on import or execution. After the one attribute line was added, the same command exited 0 and reported a pass for both `core.autocrlf=true` and `false`. Raw outputs are in `out/part25d-task5-4472724-20260923/roundtrip-red.txt` and `roundtrip-green.txt`.

## Real checkout and hashes

Before the edit, all 29 tracked fixture working files matched their index blobs byte for byte (`git hash-object --no-filters` versus `git rev-parse :<path>`); the discrepancy list is empty. SHA256s for all 29 fixture working files were captured before and after the edit, with zero differences. A separate `GIT_INDEX_FILE` was populated from `HEAD`, the corrected `.gitattributes` was added to that index only, and `git -c core.autocrlf=true checkout-index --all --prefix=<new checkout>/` exported `out/part25d-task5-4472724-20260923/checkout-corrected`. The isolated index differed from `HEAD` only at `.gitattributes`. All 29 fixture files in that actual Git checkout have the same SHA256s as the original working files; the three hash inventories and discrepancy list are retained in that scratch directory. The current branch and global Git settings were not changed.

Against the new checkout, `python scripts/generate_tgs_compatibility_corpus.py --root out/part25d-task5-4472724-20260923/checkout-corrected --check` exited 0 and verified 16 assets. The unchanged `tests/tgs_runtime_tests.cpp` was directly compiled with the new checkout's fixture macros and stable `out/build/windows-msvc-telegram-debug` libraries, using the command in `run-runtime-corrected.cmd`. Its executable printed `AveMotion TGS runtime integration tests passed` and exited 0, including its decoded JSON source-identity assertion. Raw output is in `corpus-check.txt` and `runtime-corrected.txt` beside the script.

## Project verification and limits

Focused CTest checks for `avemotion.vendor.git_protected_bytes`, `avemotion.runtime.tgs`, `avemotion.validation.tgs_corpus_integrity`, and `avemotion.validation.compatibility_corpus` passed 4/4. `python scripts/verify_vendor.py` exited 0 for Telegram and Samsung. A plain PowerShell full CTest run passed 64/65 but its `avemotion.cmake.legacy_subproject` test could not find C/C++ compilers because the developer environment was absent. Repeating the entire Telegram Debug suite through `VsDevCmd.bat` passed 65/65, exit 0. Logs are `ctest-focused.txt`, `vendor-verify.txt`, `ctest-full-telegram-debug.txt`, and `ctest-full-msvc-env.txt` in the same scratch directory.

This task verifies byte transport, corpus integrity, and TGS source identity for the checked Debug configuration. The direct compilation reused existing libraries. It makes no fresh renderer, Release, installed-consumer, or performance claim.
