# rlottie render-tree state coupling

## Observation

Repeated use of the public `Animation::renderTree()` API is not fully isolated
from the CPU raster path's mutable drawable lifecycle.

The relevant sequence in the pinned Telegram lineage is:

```text
LOTCompItem::update(frame, viewport)
LOTCompItem::buildRenderTree()
LOTDrawable::sync()
    dashed path may replace mPath in-place

CPU path:
LOTCompItem::render()
VDrawable::preprocess()
    rasterizes/moves mPath
    clears Path dirty state
```

`LOTDrawable::sync()` publishes pointers from the current `VPath` into
`LOTNode`. The CPU preprocess stage later consumes that same mutable path.
Additionally, dash expansion inside `sync()` can become history-dependent if a
runtime object is repeatedly inspected without the normal preprocess lifecycle.

## Reproduced symptom

The initial Part 3 implementation reused one `rlottie::Animation` for the
sequence:

```text
frame 0 -> middle -> same middle -> last -> frame 0
```

The final frame-zero evaluated-scene fingerprint differed from the initial
frame-zero fingerprint on corpus assets containing affected path state. Pixel
oracle tests remained deterministic because the normal CPU path executes
`preprocess()`.

## Part 3 containment

- each exact evaluated-scene sample receives a fresh runtime item tree;
- the parsed model remains shared through rlottie's model cache;
- CPU rendering uses a separate, lazily created runtime object;
- all published paths are deep-copied before the upstream object can mutate.

Direct-seek, repeated-sample and independent-instance tests now pass for all
corpus assets on Telegram and Samsung lineages, Clang and GCC, including ASan.

## Future resolution options

1. add a non-consuming upstream recording lifecycle with its own persistent path
   storage;
2. split evaluated drawables from CPU preprocess state;
3. extract the evaluator into AveMotion-owned canonical buffers;
4. retain fresh runtime-tree sampling only as the reference oracle.

Part 3 chooses option 4 until characterization coverage is sufficient for a
semantic replacement.
