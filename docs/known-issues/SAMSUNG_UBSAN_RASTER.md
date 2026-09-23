# Samsung rlottie: remaining observational UBSan raster diagnostics

The vendored Samsung source is based on commit
`2cab35db755b0e39df40b679969495e90d39c578` plus the narrowly scoped arena
hardening patch recorded in `patches/samsung/`.

## Passing required profiles

The patched source passes:

- Clang Debug and GCC Release characterization;
- compiler parity across 40 normalized corpus samples;
- the complete AddressSanitizer test suite;
- repeated frame, random seek, independent-instance, parallel independent
  render, and characterizer smoke tests;
- pixel/metadata parity against the unpatched Samsung source for all 40 sampled
  corpus frames.

## Observational UBSan profile

A non-gating Clang UndefinedBehaviorSanitizer run was performed with `vptr` and
`function` checks disabled because upstream is compiled with `-fno-rtti` and
contains FreeType-derived callback patterns unsuitable for those checks.

After the arena hardening patch, the tests complete successfully and the former
arena null-pointer diagnostic is gone. UBSan still reports signed integer
overflow in extreme boundary arithmetic in:

- `src/vector/freetype/v_ft_raster.cpp:1391-1392`;
- `src/vector/vrect.h:34` while constructing the corresponding extreme bounding
  rectangle.

The complete trace is preserved in:

`docs/characterization/linux-clang-samsung-ubsan-observe-run.log`

## Policy

- The remaining diagnostics are treated as upstream investigation items, not
  silently suppressed or falsely marked fixed.
- Any future raster arithmetic change requires focused boundary tests, the full
  corpus, ASan, UBSan, compiler parity and accepted pixel-manifest comparison.
