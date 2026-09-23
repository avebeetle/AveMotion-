# AveMotion asset validation

Part 23 adds a backend-neutral validation layer between canonical model
construction and product use.  It does not change Telegram parsing,
evaluation, geometry or Direct2D rendering.

```text
JSON / TGS
    -> hardened transport decode
    -> Telegram parsed model
    -> AveMotion canonical MotionAssetModel
    -> AssetValidator
       -> structural validity
       -> feature inventory
       -> complexity summary
       -> profile compliance
       -> deterministic report fingerprint
```

## Public target and header

```cmake
target_link_libraries(my_tool PRIVATE AveMotion::Validation)
```

```cpp
#include <avemotion/validation/AssetValidator.hpp>

avemotion::validation::AssetValidator validator;
auto report = validator.validate(
    assetModel,
    context,
    avemotion::validation::AssetValidationOptions::applicationAsset());
```

The public header contains no `rlottie`, Win32, Direct2D or decompressor types.

## Profiles

### Application asset

Accepts structurally valid assets intended for a trusted application pipeline.
It records whether features are native, native only in a proven subset,
currently delegated to the preserved Telegram reference path, or unsupported.

### Direct2D native

Requires every used feature to be fully supported by the current AveMotion
native evaluator/geometry/Direct2D path.  A reference-fallback or subset-only
feature rejects this profile.

The result is intentionally conservative.  For example, Trim Path and Repeater
are marked `NativeSubset` because only characterized forms are currently
promoted to the native path.

### Telegram sticker

Adds the Telegram animated-sticker authoring constraints to structural
validation:

- TGS transport;
- 512x512 canvas;
- bounded compressed size;
- 30–60 FPS, with 60 FPS preferred;
- maximum three-second authored duration;
- explicit loop intent;
- forbidden-feature diagnostics.

The validator also reports that static authored-model inspection cannot yet
prove that all animated bounds remain inside the canvas.

## Structural checks

The structural pass verifies, among other invariants:

- schema and direct parsed-model availability;
- finite dimensions, frame rate and timeline;
- dense typed IDs and valid table references;
- valid child/property/segment ranges;
- structural-parent and transform-parent acyclicity;
- composition-reference acyclicity;
- property/track/segment type compatibility;
- ordered and finite segment data;
- valid typed-value references;
- canonical shape point topology;
- bounded hierarchy, track, segment, point and repeater complexity.

A structurally corrupt model is rejected independently of the selected product
profile.

## Feature classification

Current categories include compositions/precompositions, layer kinds, source
paths, generated primitives, fills, strokes, gradients, Trim Path, Repeater,
masks, mattes, blend modes, auto-orient, spatial position, time stretch and
time remap.

The classification describes the current AveMotion implementation, not the
full theoretical capability of Direct2D or `rlottie`.

## Deterministic reports

`AssetValidationReport` contains:

- acceptance and structural flags;
- native/fallback/unsupported counts;
- feature occurrences;
- typed issues with node/property IDs;
- complexity counters;
- deterministic FNV-1a report fingerprint.

The fingerprint hashes canonical fields, not C++ object bytes or platform
padding.  Clang and GCC produce byte-identical corpus manifests.

## CLI

When the Telegram reference build is enabled:

```bash
avemotion_validate --profile application animation.tgs
avemotion_validate --profile direct2d animation.json
avemotion_validate --profile telegram --loop sticker.tgs
avemotion_validate --tsv --output report.tsv *.tgs
```

Exit codes:

```text
0  loaded and accepted
1  command-line, file, transport, parser or model-preparation failure
2  loaded successfully but rejected by the selected validation profile
```

## Scope boundary

Part 23 does not claim complete semantic validation of every Lottie operation.
It validates the canonical information currently extracted from the pinned
Telegram model and fails closed on unknown or unsupported source nodes.

It also does not replace the pixel, evaluated-scene, render-plan or native
Direct2D capture oracles.  Validation is an additional gate, not a substitute
for rendering parity.
