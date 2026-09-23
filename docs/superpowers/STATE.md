# AveMotion autonomous work state

Updated: 2026-09-23

## Repository

- Branch: `main`
- Remote: `https://github.com/avebeetle/AveMotion-.git`
- Baseline commit: `cda415c`
- Baseline provenance: all 2,478 files matched
  `C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24.zip` byte-for-byte.
- Prior Git history: unavailable locally and absent from the previously empty
  remote; `cda415c` intentionally begins new history.

## Current stage

The Part 25A written design is ready for user review:

`docs/superpowers/specs/2026-09-23-persistent-reference-sessions-design.md`

The design scopes the first implementation to persistent reference sessions,
session diagnostics, parity/concurrency tests, and measured performance
evidence. No product implementation is allowed until the user approves this
written design. After approval, the next action is to create and self-review the
detailed implementation plan with the `writing-plans` workflow, present it for
review, and then execute the user-selected method.

## Fresh baseline verification

- Configure: `cmake --preset windows-msvc-telegram-debug` — passed with
  MSVC 19.44.35229.0.
- Build: `cmake --build --preset windows-msvc-telegram-debug --parallel 4`
  — passed. The first parallel run stopped while linking one test executable
  without a linker diagnostic; an isolated verbose reproduction and the full
  incremental build both passed, so the initial failure's cause is unconfirmed.
- Tests: `ctest --preset windows-msvc-telegram-debug --output-on-failure` —
  57/57 passed in 65.55 seconds.

## Duplicate-run rule

Scheduled and interactive runs must read this file before acting. While the
stage is `written design awaiting user review`, do not create a second spec,
implementation plan, branch, worktree, or product-code change. Report the
review request once, then remain quiet until user input changes the stage.
