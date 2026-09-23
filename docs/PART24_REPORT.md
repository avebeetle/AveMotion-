# AveMotion Part 24 — private/external TGS corpus laboratory

## Executive result

Part 24 deliberately adds no new Lottie rendering family. It adds the evidence
layer needed to choose Part 25 from representative `.tgs` and Lottie JSON
assets instead of guessing.

The stage delivers a privacy-preserving corpus scanner, safe ZIP runner,
feature-frequency reports, three validation profiles, current native/fallback
coverage, load/evaluation/CPU-reference timing, retained memory estimates and a
deterministic feature-priority report.

The input animations are never copied into the report directory. The default
report contains only stable content-derived aliases. Base file names are
included only with the explicit `--include-names` option.

## Pipeline

```text
private directory or ZIP
        ↓
safe enumeration / bounded ZIP extraction
        ↓
TGS or JSON detection
        ↓
hardened TGS decoding
        ↓
Telegram parser/model reference path
        ↓
AveMotion canonical model
        ↓
AssetValidator
├── ApplicationAsset
├── Direct2DNative
└── TelegramSticker
        ↓
AveMotion evaluator
        ↓
SourceGeometryProjector
        ↓
MotionRenderPlan
        ↓
feature, timing, memory and priority reports
```

## New target and tools

```text
avemotion_corpus_analysis   internal deterministic analysis library
avemotion_corpus_lab        corpus scanner and report generator
```

Runner commands:

```bash
python scripts/run_part24_corpus_lab.py \
  --input-dir C:/stickers \
  --output C:/reports/avemotion-corpus
```

```bash
python scripts/run_part24_corpus_lab.py \
  --input-zip C:/stickers.zip \
  --output C:/reports/avemotion-corpus
```

Windows wrapper:

```bat
scripts\run_part24_corpus_lab.cmd --input-zip "C:\stickers.zip" --output "C:\reports\avemotion-corpus"
```

Dedicated focused build presets:

```text
linux-gcc-corpus-lab
windows-msvc-corpus-lab
```

These presets build the Telegram reference path and corpus lab without the
unrelated characterizers, Direct2D capture application or Win32 preview.

## ZIP safety

The runner rejects:

- absolute member paths;
- `..` traversal;
- symbolic links;
- more than 1000 entries;
- more than 64 MiB expanded data.

Extraction is temporary and deleted after the report is generated.

## Output files

```text
corpus_manifest.tsv
corpus_issues.tsv
corpus_features.tsv
corpus_benchmarks.tsv
corpus_decision.tsv
corpus_summary.txt
```

### `corpus_manifest.tsv`

Per-asset metadata and state:

- stable alias;
- format;
- source/content hashes;
- encoded and JSON sizes;
- dimensions, frame rate, frame count and duration;
- application-profile acceptance;
- fully native Direct2D readiness;
- reference-fallback requirement;
- unsupported-feature state;
- Telegram-profile acceptance;
- canonical table sizes and complexity.

### `corpus_issues.tsv`

Typed diagnostics for all three profiles. Each issue retains profile,
severity, diagnostic code, feature, source node/property and message.

### `corpus_features.tsv`

Occurrence count, implementation support and exact union of affected assets for
each feature. Asset unions are computed per candidate family; overlapping
features no longer double-count an asset and disjoint features are not hidden by
a conservative maximum.

### `corpus_benchmarks.tsv`

Measurements and estimates:

- median load + canonical-model construction;
- first full native pipeline sample;
- steady pipeline average;
- Telegram CPU-reference render median;
- projected/source/unsupported draw items;
- canonical model memory estimate;
- evaluator retained workspace;
- geometry-projector retained workspace;
- evaluated scene and render plan storage;
- approximate one-, sixteen- and sixty-four-instance memory totals.

Measurements are characterization data, not a final performance guarantee.
They are intended for comparing assets and identifying outliers under the same
machine/build configuration.

### `corpus_decision.tsv`

Candidate families are ranked using:

```text
blockedAssets * 1000
+ occurrences * 25
+ supportWeight
- estimatedEffort * 50
```

The result is explicitly marked preliminary until the input corpus represents
the intended product.

## Deterministic smoke corpus

The committed 16-asset TGS corpus exercises the complete report pipeline. It is
made from existing project fixtures and is not claimed to represent the public
Telegram sticker ecosystem.

Result:

```text
files=16
loaded=16
failed=0
nativeDirect2DReady=6
fallbackAssets=5
unsupportedAssets=0
telegramProfileAccepted=1
sourceAssetsCopied=0
```

Preliminary ranking:

| Rank | Candidate | Blocked assets | Occurrences | Score |
|---:|---|---:|---:|---:|
| 1 | native-subset-completion | 6 | 27 | 6525 |
| 2 | masks-and-mattes | 3 | 4 | 2700 |
| 3 | nested-composition-time | 2 | 24 | 2400 |
| 4 | layer-and-media-support | 2 | 2 | 1750 |
| 5 | gradients | 1 | 1 | 975 |

This ranking is useful only as a smoke result. Part 25 must be selected after a
representative private or redistributable external corpus is scanned.

## Retained-memory diagnostics

Part 24 adds read-only estimates:

```cpp
PropertyEvaluationWorkspace::retainedBytes()
SourceGeometryProjectionWorkspace::retainedBytes()
```

They expose reserved retained storage without changing evaluator or projector
semantics. No platform renderer types enter the API.

## Privacy invariants

Default mode guarantees:

```text
source path written to reports: no
source base name written:       no
source .tgs/.json copied:       no
stable content alias:           yes
```

`--include-names` is explicit opt-in and records only a sanitized base name.

## Validation matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang 17 Debug | 56/56 PASS |
| Telegram + GCC 14 Release | 56/56 PASS |
| Telegram + Clang 17 ASan, tests 1–36 | 36/36 PASS |
| Telegram + Clang 17 ASan, tests 37–56 | 20/20 PASS |
| Samsung + GCC 14 Release | 37/37 PASS |
| Standalone/offline Clang | 27/27 PASS |
| Dedicated ZIP runner against 16 TGS | PASS |
| Safe ZIP traversal/absolute-path tests | PASS |
| CMake install/export 0.24.0 | PASS |
| External `find_package(AveMotion 0.24)` consumer | PASS |
| Vendor integrity | PASS |

The ASan suite was executed in two bounded CTest ranges because a combined tool
invocation exceeded the external command-duration limit. Every one of the 56
tests passed under the same ASan build.

## Existing behavior preserved

Part 24 does not change:

- Telegram or Samsung vendored source trees;
- TGS decoding semantics;
- canonical model contents;
- property evaluation;
- Shape, primitive, Trim or Repeater geometry;
- MotionRenderPlan semantics;
- Direct2D rendering and cache keys;
- Player scheduling;
- Win32 preview;
- previous CPU, evaluated-scene, render-plan and Direct2D capture gates.

## Honest limitation

No private user sticker pack was available while Part 24 was built. Therefore
the committed decision report is a deterministic fixture result, not a claim
about the most common feature in real Telegram packs.

The lab is now ready to answer that question without adding the source assets to
the AveMotion repository or to generated reports.

## Part 25 gate

Run the laboratory on approximately 10–30 representative `.tgs` files and
review:

```text
corpus_decision.tsv
corpus_features.tsv
corpus_issues.tsv
corpus_benchmarks.tsv
```

Only then choose one isolated Part 25 objective, such as:

- completion of remaining native Trim/Repeater subsets;
- nested composition and local-time mapping;
- gradients;
- dashed strokes;
- masks/mattes;
- non-shape layers.

The development rule remains:

```text
characterize -> isolate -> implement -> compare -> preserve oracle
```
