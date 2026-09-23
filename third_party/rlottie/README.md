# Vendored rlottie reference implementations

This directory contains the two pinned source archives used by the extraction laboratory.

## Primary: Telegram

- Repository: `https://github.com/TelegramMessenger/rlottie`
- Commit: `67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d`
- Path: `telegram/source`
- Role: primary behavioral baseline and initial extraction target
- Local edits: the cumulative build/MSVC patch and additive Part 5
  local-geometry metadata seam documented under `patches/telegram/`
- Archive normalization: excludes the accidental non-source Vim swap file
  `src/binding/.lottieplayer.cpp.swp`; the original archive hash remains pinned.

## Comparison: Samsung

- Repository: `https://github.com/Samsung/rlottie`
- Commit: `2cab35db755b0e39df40b679969495e90d39c578`
- Path: `samsung/source`
- Role: maintained comparison, security-hardening and modernization donor
- Local edits: the sanitizer-hardening patch documented under `patches/samsung/`
- Remaining sanitizer note: `docs/known-issues/SAMSUNG_UBSAN_RASTER.md`

The source trees intentionally retain upstream names and directory layouts.
Only private reference/runtime bridge implementation files may include upstream
headers. Public AveMotion headers and new production modules use AveMotion-owned
contracts.

`UPSTREAM.json` records the source archive hashes, post-patch deterministic tree
fingerprints, file counts, byte counts, commits and local patch lists. Run:

```bash
python3 scripts/verify_vendor.py
```

Licenses and notices inside each source tree must remain intact.
