# Source-geometry raw-float fingerprint portability

## Trigger

A real MSVC Part 20.3 run passed the complete native product path, including:

```text
Win32 preview lifecycle
D3D11 WARP smoke
75-case Direct2D capture corpus
Telegram property/geometry parity
scene and render-plan goldens
```

The remaining failure was one source-geometry row:

```text
asset:    polystar_polygon_geometry.json
sample:   p000
frame:    0
```

Every structural and semantic field matched. Only the aggregate
`projected_fingerprint`, which includes exact raw `float` geometry bits,
differed:

```text
Linux golden: 7eee63a3feae85a5
MSVC:         c2aef5615ae7f285
```

This second case demonstrates that a row-by-row list of raw hashes is the wrong
portability contract for source geometry. Legacy Telegram `rlottie` and the
AveMotion Telegram-compatible primitive generators contain single-precision
trigonometry and recursive path arithmetic whose least-significant bits may
differ between libm and the MSVC CRT.

## Part 20.4 policy

The characterizer now emits two independent fingerprints.

### Raw fingerprint

`projected_fingerprint` preserves exact IEEE-754 bits and runtime-derived raw
revisions. It remains strict on the toolchain that authored the persisted
golden and is still valuable for detecting same-platform drift.

### Portable fingerprint

`projected_portable_fingerprint` is recomputed from:

- exact source path/paint IDs;
- exact geometry origin and fill rule;
- exact path verb stream;
- path coordinates rounded to a 1/64 asset-unit fixed-point grid;
- exact Repeater source/copy metadata.

It deliberately does not reuse raw geometry hashes or raw content-derived
revisions. The grid is substantially finer than a display pixel in the current
corpus and only removes low-order CRT noise.

On MSVC, a raw-fingerprint mismatch is accepted only if the portable
fingerprint and every other TSV field remain exact. A different portable path,
point count, topology, feature count, projection count, geometry scope or
update statistic still fails.

## Runtime remains exact

This policy is confined to persisted characterization manifests. It does not
change:

- evaluated coordinates;
- `EvaluatedPath::hash`;
- geometry or paint revisions;
- `MotionRenderPlan` identities;
- Direct2D cache keys;
- dirty tracking;
- rendering or capture tolerances.

The production engine therefore remains bit-exact within one execution and
continues rebuilding resources whenever its actual exact geometry changes.
