# License and security notes

## Telegram primary baseline

The vendored TelegramMessenger/rlottie tree identifies its core under
LGPL-2.1-or-later and includes separate license texts for bundled
FreeType-derived, pixman, stb and other components. Modifying and redistributing
a derivative fork creates source, notice and relinking obligations that must be
reviewed before a public AveMotion release.

## Samsung comparison baseline

The supplied Samsung snapshot identifies the rlottie core under MIT and includes
separately licensed bundled components under `licenses/`. Samsung is a
comparison and hardening donor, not a silent behavioral replacement.

## New AveMotion code

No final license is selected for AveMotion-owned model/runtime/evaluation/
rendering code, tests or tools. Choose one before public distribution and perform
a file-level audit of every upstream file that remains.

`AveMotion::Evaluation` is AveMotion-owned code and has no link dependency on
Telegram or Samsung in the offline/installable configuration. Reference builds
still link the selected rlottie tree for oracle tests and complete rendering.

## Telegram introspection patch

`0004-avemotion-parsed-model-introspection.patch` is an additive read-only seam.
It exposes parsed transform data, authored temporal Bezier controls, property
values and an explicit time-remap-presence flag. It does not replace normal
parser/evaluator/raster behavior, but it is still a modification of
LGPL-covered upstream code and must remain source-available with its notices if
distributed under that model.

Part 9 adds no further Telegram source patch. Exact-property comparison uses the
existing private introspection seam.

## Security posture

This laboratory is not yet a sandbox for arbitrary untrusted animations. The
canonical model and Part 9 evaluator validate:

- compositions, source nodes, properties, tracks and segments;
- graph IDs and all array ranges;
- static/animated property invariants;
- property/track/value type agreement;
- contiguous ordered segment ranges;
- finite scalar, vector, color, matrix, path and gradient data;
- temporal and spatial control values;
- model/workspace identity before evaluation;
- finite requested frame values.

These checks do not cover every production threat. A future importer/compiler
must also enforce JSON size/nesting, decompression limits, dimensions, total
path points, images, masks/effects, offscreen memory and total evaluation cost.

Current policy:

- trusted pinned corpus only;
- exact vendored-source fingerprints verified before build;
- AddressSanitizer runs for Telegram and Samsung paths;
- no production loading of arbitrary network/user assets;
- every parser/model/evaluator change requires malformed-input and golden/parity
  coverage;
- unsupported semantics are rejected or marked unsupported, never guessed.


## Part 9 spatial hardening

Telegram's unbounded spatial `tAtLength` search is not copied literally.
AveMotion uses the same numerical update rule and tolerance with a hard limit of
256 iterations and an explicit `SpatialSearchDidNotConverge` error. Hierarchy
construction also rejects self-parenting, invalid IDs, cross-composition edges
and cycles before evaluation.


## Part 11 source-geometry binding

`0005-avemotion-source-geometry-binding.patch` adds read-only source-node IDs,
path count and modifier-free metadata to the Telegram runtime extraction seam.
It does not replace or alter the legacy evaluated path, rasterizer or compositor,
but it remains a modification of LGPL-covered upstream code and must remain
source-available with the applicable notices when distributed.

Source projection is fail-closed: unsupported bindings, modifiers, unavailable
transforms and point-level parity mismatches keep the Telegram geometry path.

## Part 12 primitive path compatibility

The Rectangle/Ellipse generator independently lives in AveMotion-owned source,
but intentionally reproduces the pinned Telegram/Samsung LGPL path semantics,
including historical constants and float-operation order needed for behavioral
parity. The vendored original sources, notices and LGPL text remain preserved.
Any future redistribution must continue to satisfy the project's documented
third-party and derivative-work obligations.

## Part 13 Polystar/Polygon compatibility

The AveMotion Polystar/Polygon generator is AveMotion-owned source, but it
intentionally reproduces the pinned Telegram `VPath::addPolystar()`,
`VPath::addPolygon()` and `LOTPolystarItem` numerical behavior. This includes
historical float constants, fractional-point handling, polygon angle arithmetic
and the Telegram baseline's double application of the authored rotation.

Generation is bounded to 256 authored points in this laboratory. Larger or
invalid values fail closed and retain Telegram's established geometry. Every
published generated path is still compared verb-for-verb and point-for-point
against the preserved Telegram local path. Redistribution must retain the
project's documented third-party/derivative-work notices and receive a
file-level licensing review.

## Part 14–15 Trim Path compatibility

`src/render/TrimPathGenerator.cpp` is an AveMotion backend-neutral adaptation of
observable algorithmic behaviour from LGPL-covered Telegram/Samsung rlottie
components including `LOTTrimData`, `VPathMesure`, `VDasher`, `VBezier` and
`VLine`. The file carries an explicit LGPL-2.1-or-later derivative-work notice.
The original source snapshots and license texts remain vendored and the
adaptation must receive file-level licensing review before public SDK
redistribution.

The compatibility implementation adds defensive bounds absent from some legacy
paths: finite-value validation, maximum Bézier recursion depth and maximum
length-search iterations. Part 15 extends the same derivative implementation to
Telegram-compatible multi-path Simultaneous and aggregate-length Individual
semantics. Failure is fail-closed to the existing Telegram geometry rather than
publishing an approximate path.

## Part 16 Repeater compatibility

`src/render/RepeaterTransform.cpp` is AveMotion-owned source that intentionally
reproduces observable numerical behaviour from LGPL-covered Telegram rlottie
components `LOTRepeaterTransform`, `LOTRepeaterItem` and `VMatrix`. This includes
operation order, exponential scale, special-angle rotation branches,
`int(copies)` visibility and per-copy opacity interpolation. The original
vendored source and license texts remain preserved; the new implementation must
receive file-level licensing review before public redistribution.

Repeater extraction is fail-closed. Unsupported group structures, missing or
non-finite properties, copy counts beyond configured limits, path/matrix/opacity
parity failures and incomplete local paint seams preserve Telegram's original
evaluated geometry. The dedicated fixture exercises all 61 source frames and
verifies bounded retained workspace storage.

## Part 17 Repeater content-group compatibility

Part 17 extends the AveMotion-owned compatibility implementation to reproduce
Telegram's direct content-group paint traversal, local solid fill/stroke values
and Telegram-compatible stroke scaling. The implementation was derived from
observable behaviour and the preserved LGPL-covered Telegram source family.
The vendored source and license texts remain intact; the new implementation
requires the same file-level licensing review before public SDK redistribution.

The extension remains fail-closed. Chained Repeaters, Trim/Merge combinations,
gradients, dashed strokes, masks/mattes, invalid ancestry, non-finite property
values and any path/matrix/paint parity mismatch preserve Telegram's original
state. The exhaustive 61-frame fixture also verifies bounded retained workspace
storage and deterministic geometry/paint identities.

## Part 22 TGS container and miniz

Part 22 vendors the public-domain/Unlicense `miniz.c 2.2.0` snapshot already
present in the Samsung comparison source. Its notice is preserved in
`third_party/miniz/miniz.h` and `LICENSES/UNLICENSE-miniz.txt`. The code is
compiled privately with AveMotion-prefixed low-level symbols and is not exposed
through SDK headers.

The TGS decoder treats container bytes as untrusted. Before output allocation it
enforces compressed-size, decompressed-size and expansion-ratio limits. It
bounds all optional gzip header fields, optionally validates FHCRC, requires one
complete raw-DEFLATE stream, rejects trailing/concatenated data, verifies ISIZE
and CRC-32, validates UTF-8 and requires a JSON object envelope. The default
profile is 64 KiB compressed, 2 MiB decompressed and 128:1 expansion; larger
trusted application assets require explicit opt-in.

These checks harden the container boundary, but the downstream pinned rlottie
JSON parser remains legacy code and is not a general sandbox. Production use
with arbitrary network assets still requires a complete canonical
validator/complexity policy and continued malformed-input testing.
