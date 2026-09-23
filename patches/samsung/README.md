# Samsung reference hardening patch

Part 2 applies one narrowly scoped sanitizer-driven patch to the supplied
Samsung commit.

The Skia-derived arena supports a null optional first block, but the pinned code
performed pointer arithmetic before allocating its first heap block and reused
members after explicitly ending the allocator object's lifetime in `reset()`.

The patch:

- avoids `nullptr + 0` in the constructor;
- allocates the first block before alignment/subtraction arithmetic;
- copies reset constructor arguments before ending the object's lifetime.

Validation performed before adoption:

- full Clang/GCC corpus matrix passes;
- AddressSanitizer passes all Part 2 tests and the 40-frame corpus;
- the arena UBSan diagnostic is removed;
- 40/40 Samsung characterization samples are pixel- and metadata-identical to
  the unpatched source;
- no Lottie timing, property, geometry, rasterization or compositing change is
  intended.

Remaining FreeType-derived signed-overflow UBSan diagnostics are documented in
`docs/known-issues/SAMSUNG_UBSAN_RASTER.md` and are not hidden by this patch.
