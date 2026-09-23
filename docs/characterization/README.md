# Characterization artifacts

This directory records eight independent boundaries:

1. CPU-pixel output;
2. evaluated vector scenes before CPU rasterization;
3. backend-neutral render plans;
4. canonical local-space provenance and generation identities;
5. immutable render-facing typed asset tables and host-driven playback;
6. direct parsed-model compositions, properties, tracks and segments;
7. independent spatial/property/world-transform evaluation with Telegram parity;
8. retained animated Shape-property materialization with point-level Telegram parity.

## Variants

- `telegram-*`: primary behavior, TelegramMessenger/rlottie commit
  `67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d` plus documented build and
  additive extraction patches;
- `samsung-*`: comparison/hardening only, Samsung/rlottie commit
  `2cab35db755b0e39df40b679969495e90d39c578` plus documented arena hardening.

## Persistent goldens

```text
tests/golden/scene-telegram.tsv
tests/golden/scene-samsung.tsv
tests/golden/plan-telegram.tsv
tests/golden/plan-samsung.tsv
tests/golden/model-telegram.tsv
tests/golden/parsed-model-telegram.tsv
tests/golden/property-telegram.tsv
```

`property-telegram.tsv` is the Part 10 five-sample-per-asset oracle. It records
property, spatial, Shape, local-transform and world-transform fingerprints plus
lookup, spatial-search and topology diagnostics. Supported values and every
materialized Shape point are compared field by field during generation.

The broader non-persisted parity test samples 213 boundaries/midpoints/random
frames across the corpus and a dedicated auto-orient fixture, exercising
forward, reverse and direct-seek traversal. Part 10 compares 165 animated
Shape samples and 3,324 path points against Telegram.

## Part 10 reports and logs

- `docs/PART10_REPORT.md`;
- `docs/SHAPE_EVALUATION.md`;
- `docs/PROPERTY_EVALUATOR.md`;
- `docs/WORLD_TRANSFORMS.md`;
- `docs/build-logs/part10/`.

CPU, scene, plan and parsed-model goldens remain byte-identical to Part 9. The
property golden changes intentionally because animated Shapes are now owned and
materialized by AveMotion.
