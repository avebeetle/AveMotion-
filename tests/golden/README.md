# AveMotion golden manifests

The committed corpus has six persisted regression boundaries plus runtime,
playback and broad parity tests.

## CPU pixels

Artifacts under `docs/characterization/` capture ARGB surface hashes, alpha
sums and non-transparent bounds from the unchanged rlottie CPU renderer.

## Evaluated scenes

`scene-telegram.tsv` and `scene-samsung.tsv` capture hierarchy, geometry, paint,
masks/mattes and structural counts before CPU rasterization.

## Render plans

`plan-telegram.tsv` and `plan-samsung.tsv` capture backend-neutral draw identity,
resource revisions, presentation fingerprints, updates and unsupported feature
counts.

## Immutable asset-model summary

`model-telegram.tsv` records one model summary per corpus asset, including both
legacy render-facing tables and direct parsed-model table counts.

## Detailed parsed model

`parsed-model-telegram.tsv` records the complete deterministic authored structure:

- compositions and reusable precomposition references;
- source nodes and non-animatable metadata;
- typed properties and component indices;
- tracks and segments;
- exact numeric bit patterns for scalar, vector, color and matrix values;
- hashes/ranges for shape and gradient values.

CTest targets:

```text
avemotion.model.golden
avemotion.model.parsed_golden
```

Samsung intentionally has no direct parsed-model golden because it does not
expose the Telegram introspection seam.

## Property evaluation

`property-telegram.tsv` records five exact source samples per asset for the
independent Part 10 evaluator. It contains:

- total, compared, unsupported, spatial-property and animated-Shape counts;
- AveMotion and Telegram property fingerprints;
- local-transform and world-transform oracle fingerprints;
- cursor, adjacent-search and binary-search counts;
- temporal cubic and spatial sample counts;
- spatial length-search totals and maximum;
- materialized Shape point totals, truncation diagnostics and Shape changes;
- world-transform changes.

CTest targets:

```text
avemotion.evaluation.characterize
avemotion.evaluation.golden
avemotion.evaluation.telegram_parity
```

Every supported property, local transform and world transform is compared field
by field. Animated Shape values are compared point-for-point with Telegram; animated Gradient values remain explicit unsupported values.

Current property-golden SHA-256:

```text
e2fd5678112a997bc7dd6f23a3ccdf1ee575199478c24dd6a246c8fd0debe5a4
```

## Deliberate regeneration

```bash
./out/build/linux-clang-telegram-debug/avemotion_scene_characterize \
  --output /tmp/scene-telegram --size 128 tests/corpus/*.json
cp /tmp/scene-telegram/scene_manifest.tsv tests/golden/scene-telegram.tsv

./out/build/linux-clang-telegram-debug/avemotion_plan_characterize \
  --output /tmp/plan-telegram --size 128 tests/corpus/*.json
cp /tmp/plan-telegram/plan_manifest.tsv tests/golden/plan-telegram.tsv

./out/build/linux-clang-telegram-debug/avemotion_model_characterize \
  --output /tmp/model-telegram tests/corpus/*.json
cp /tmp/model-telegram/model_manifest.tsv tests/golden/model-telegram.tsv
cp /tmp/model-telegram/parsed_model_details.tsv \
  tests/golden/parsed-model-telegram.tsv

./out/build/linux-clang-telegram-debug/avemotion_property_characterize \
  --output /tmp/property-telegram tests/corpus/*.json
cp /tmp/property-telegram/property_manifest.tsv \
  tests/golden/property-telegram.tsv
```

A golden failure is never fixed by refreshing files without a written semantic
reason and review of the detailed diff.

## Source geometry, modifiers and Repeater

`source-geometry-telegram.tsv` records 70 samples: the original eight-asset
corpus plus `primitive_geometry.json`, `polystar_polygon_geometry.json`,
`trim_path_geometry.json`, `multi_trim_path_geometry.json`,
`repeater_geometry.json` and `repeater_content_group.json`.

Part 17 retains the Part 16 Repeater diagnostics and adds explicit columns for:

- accepted solid Fill applications;
- accepted solid Stroke applications;
- nested-group paint applications;
- paint applications with animated canonical properties.

The Trim and Repeater fixtures are also checked exhaustively on all 61 source
frames, not only the five persisted positions. Prepared projection workspaces
must not grow after `prepare()`. Repeater copies sharing one authored geometry
binding must use the same geometry cache key; copies of one authored paint must
use the same paint cache key.

The 65 pre-Part-17 rows retain every previous field value. Part 17 adds four
columns and five rows for the new content-group fixture.

Current SHA-256:

```text
bd42276cf5e098b8219b5cb6c919d7eeb817bd1cbeac70ece4fde733dfb8628f
```

## Cross-platform legacy-oracle variance

The persisted Telegram scene, render-plan and source-geometry manifests retain
exact raw-float fingerprints. Scene and render-plan comparison still has one
narrow exact captured MSVC exception:

```text
polystar_line_clockwise_trim.json / p100 / frame 149 / 128x128
```

Source geometry now carries an additional
`projected_portable_fingerprint`. It hashes exact topology/source identities
and path coordinates quantized to 1/64 asset unit. On MSVC only, the raw
`projected_fingerprint` may differ when the portable fingerprint and every
other field remain byte-exact. Linux Clang/GCC continue to require the raw
fingerprint as well.

The policy is guarded by:

```text
avemotion.golden.cross_platform_policy
avemotion.golden.source_geometry_policy
```

See:

- `docs/known-issues/RLOTTIE_MSVC_POLYSTAR_ENDPOINT_FLOAT_VARIANCE.md`
- `docs/known-issues/SOURCE_GEOMETRY_RAW_FLOAT_PORTABILITY.md`
