# AveMotion Part 23 — canonical asset validation and deterministic TGS compatibility corpus

## Objective

Part 22 made `.tgs` a safe first-class input.  Part 23 adds the product-facing
answer to a different question:

> Can this decoded asset be trusted structurally, which features does it use,
> and can the current native Direct2D path render it without reference
> fallback?

No new Lottie rendering feature is introduced in this stage.

## Delivered architecture

```text
TGS / JSON
    -> Part 22 bounded loader
    -> pinned Telegram parser/model
    -> AveMotion canonical model
    -> AveMotion::Validation
       -> structural validation
       -> feature inventory
       -> profile policy
       -> complexity and deterministic diagnostics
```

New installed target:

```text
AveMotion::Validation
```

New tools/tests:

```text
avemotion_validate
avemotion_validation_characterize
avemotion.validation.asset
avemotion.validation.compatibility_corpus
avemotion.validation.tgs_corpus_integrity
avemotion.validation.characterize
avemotion.validation.golden
avemotion.cmake.validation_wiring
```

## Profiles

- `ApplicationAsset`: structurally valid, known application asset; fallback is
  reported but allowed.
- `Direct2DNative`: accepts only the fully native current subset.
- `TelegramSticker`: verifies transport/canvas/rate/duration/loop intent and
  authoring-profile restrictions in addition to structural validity.

## Compatibility corpus

The repository now contains 16 deterministic `.tgs` files generated from the
existing characterized JSON corpus and fixtures.  This corpus is not described
as a downloaded real sticker set; it is a reproducible transport and feature
matrix.

Corpus totals:

```text
assets:                    16
canonical source nodes:   330
properties:               829
tracks:                    80
segments:                 108
canonical shape points:   400
estimated work units:    2808
```

Classification results:

```text
Application profile accepted:   16 / 16
Current Direct2D-native:          6 / 16
Reference-fallback assets:        5 / 16
Unsupported-feature assets:       0 / 16
Strict Telegram-profile assets:   1 / 16
```

Feature occurrences:

```text
native:              216
native-subset:        27
reference-fallback:   31
unsupported:           0
```

The strict fixture `telegram_sticker_basic.tgs` is 512x512, 60 FPS, one second,
loop-intended and uses only a native ellipse/fill path.  More complex fixtures
are intentionally retained to prove rejection and fallback diagnostics.

## Determinism

Generated manifests:

```text
tests/golden/validation-telegram.tsv
tests/golden/validation-features-telegram.tsv
tests/golden/validation-summary-telegram.txt
validation_issues.tsv (build evidence)
```

Final evidence SHA-256 values:

```text
validation_manifest.tsv:  e197c268de538a89dd75cc6fc84cdec45f6b125d91f1bdf1e48bf2a1c30c0ea5
validation_issues.tsv:    7116b024c7578f7933e3f5c8717a2e46eaafe76d12d28f9fc949c02387a9938e
validation_features.tsv:  8115ca8a8855683a98a4bf6d26d69b4d57bee9b20e794e3e29dec4d90a6d2c2f
validation_summary.txt:   1c53b5d2e3143778582101e140d659cc4c69d2008e764fc924d7fa1ea5fa3554
```

Clang Debug and GCC Release produced byte-identical validation, issue,
feature-frequency and summary evidence.

## CLI evidence

Strict Telegram fixture:

```text
Result: ACCEPT
Direct2D native ready: yes
Fallback occurrences: 0
Unsupported occurrences: 0
```

The existing Repeater content-group fixture is intentionally rejected by the
Telegram profile because its canvas is 800x420 and the authored profile uses a
Repeater, while it remains structurally valid for application use.

## Verification completed in the development environment

```text
Standalone/offline Clang:       24 / 24 PASS
Standalone ASan + UBSan:        24 / 24 PASS
Telegram + Clang Debug:         43 / 43 PASS
Telegram + GCC Release:         43 / 43 PASS
Samsung + GCC Release:          31 / 31 PASS
Telegram ASan validation path:  PASS through all new validation tests
CMake install/export 0.23.0:    PASS
External find_package consumer: PASS
Vendor integrity:               PASS
Corpus regeneration --check:    PASS
Clang/GCC manifests:            byte-identical
```

The full Telegram ASan aggregate was externally interrupted later at the
pre-existing source-geometry characterizer after all new Part 23 validation
tests had already passed; no sanitizer diagnostic was emitted.  The complete
standalone ASan/UBSan suite and all non-sanitized reference suites passed.

## Windows gate

`scripts/run_part23_windows.cmd` performs:

```text
configure/build
-> validation/TGS/player/Win32 focused tests
-> optional 75-case native capture
-> remaining suite
-> strict Telegram ACCEPT CLI check
-> expected forbidden-profile REJECT CLI check
-> validation-manifest row validation
-> optional interactive preview
```

It writes evidence under:

```text
out/build/windows-msvc-win32-preview/validation-artifacts/
out/build/windows-msvc-win32-preview/validation-characterize-golden/
```

## Honest boundary

The 16-file compatibility corpus is generated from the existing legal,
versioned laboratory fixtures.  It is not yet a broad real-world sticker-pack
sample.  A private rights-reviewed set of real `.tgs` files remains the next
input needed to prioritize the next semantic feature by actual frequency and
cost.

## Recommended next stage

Part 24 should ingest a user-provided private real TGS pack, run this validator
and native/reference classification, then benchmark load/evaluate/draw/memory
for 1, 16 and 64 instances.  The evidence should decide whether the next
implementation target is gradients, dashed strokes, nested composition time,
masks/mattes, or another actually frequent blocker.
