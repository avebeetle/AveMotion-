# Local reference patches

Part 2 keeps local upstream edits small, numbered, documented and covered by the
characterization matrix.

- `telegram/0001-modern-toolchain-build-only.patch`: removes global `-Werror`,
  adds a missing standard header, and backports the proven MSVC CMake fixes from
  `desktop-app/rlottie`. No intentional animation behavior change.
- `telegram/0002-avemotion-local-geometry-metadata.patch`: adds a narrowly
  scoped extraction seam for local geometry, transform separation, staticness
  proof and base solid-paint metadata while preserving the legacy CPU path.
- `samsung/0001-fix-arena-null-pointer-arithmetic.patch`: removes the
  sanitizer-observed first-allocation null arithmetic and reset-lifetime use.
  It passes ASan and preserves all 40 accepted Samsung sample hashes. Remaining
  raster diagnostics are documented in
  `docs/known-issues/SAMSUNG_UBSAN_RASTER.md`.

Future semantic patches require a dedicated rationale and golden regression test.
