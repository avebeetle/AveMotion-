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

## Part25C recording lifecycle (0002)

`0002-avemotion-recording-lifecycle.patch` adds the private C++
`Animation::enableRecordingLifecycle()` and
`renderTreeForRecording(size_t,size_t,size_t) const` seam. Enable is one-way,
pristine-only and idempotent. Each instance remains serial; separate instances
own their evaluation state. Recording rejects ordinary tree calls, raster
sync/async and property overrides before evaluation or scheduling. Raster and
property rejection throws `std::logic_error`; only `lottieanimation.cpp` enables
exception unwinding so callback-owned resources are released on rejection.

Recording resets constructor evaluation scratch recursively through the actual
arena-owned graph, retaining layers, repeater copies, bindings and resources.
It clears authored shape/mask paths for empty or gapped PathData, preserves
Samsung's inclusive out-frame and local -1 trim semantics, and restores typed
stroke/dash defaults without reallocating their payloads. Publication owns a
dashed path made by the original value-returning dasher (including all-zero
patterns), retains gradient stop ownership, and preserves image brushes.
No recording call invokes CPU preprocess or schedules raster work. Returned
trees are borrowed until the next sample or destruction; copy them immediately.

Ordinary fresh sampling remains the differential oracle and Runtime continues
to use it in Part25C. The global -1 constructor-cache no-build sentinel returns
null through the recording entry, avoiding the ordinary unbuilt-C-tree access.
Invalid dimensions return null and later valid samples recover.

The patch is relative to the pre-Part25C source at repository `45a21a4` (with
0001 already applied). Reapply with command-local `core.autocrlf=false` and
compare the six changed source files byte for byte. MSVC Debug common lifecycle
tests cover full fixture histories, guards, lifetime and independent host threads;
this is not a zero-allocation, speedup, sanitizer or cross-compiler claim.
Samsung's two pre-existing Polystar scene/plan golden failures remain unchanged.
