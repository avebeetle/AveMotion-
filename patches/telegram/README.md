# Telegram reference patches

The supplied Telegram commit predates current compiler and CMake integration
work. Patch 0001 contains build/integration changes only. Patches 0002 through 0005
add extraction metadata for AveMotion while preserving the legacy evaluator,
CPU rasterizer and compositing behavior.

Patch 0001 changes:

- remove global `-Werror` so new compiler diagnostics in vendored pixman/stb do
  not become unrelated hard failures;
- add the missing standard `<limits>` include in `vrle.cpp`;
- backport the MSVC compile-option, static-build definition, linker and install
  fixes from `desktop-app/rlottie` commit
  `b9d5f532cdaf97b77dc3fe5b50317493b7da0864`;
- guard Unix-only visibility flags in the optional stb image-loader target.

The full cumulative diff against Telegram commit
`67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d` is stored beside this file.
Linux Clang/GCC characterization must remain pixel- and metadata-identical after
these build-only edits. Windows/MSVC execution is covered by the prepared CI
matrix and must be verified on a Windows runner before release.

## 0002 — AveMotion local-geometry metadata seam

Part 5 adds an additive, AveMotion-owned metadata seam to Telegram's runtime
draw nodes. It exposes the local path already retained by rlottie, the separate
affine transform used to produce the legacy viewport path, staticness proof for
path/transform/modifier chains, base solid-paint values and inherited opacity.
The legacy `mPath`, paint and CPU rasterization fields are unchanged. Pixel and
evaluated-scene goldens must remain byte-for-byte identical. The extension is
compiled only for the Telegram baseline; Samsung remains an unmodified
comparison/hardening implementation.

## 0003 — Stable AveMotion source identities

Part 6 assigns deterministic source IDs while constructing Telegram's runtime
item tree. The additive metadata exposes stable layer, node, geometry and paint
identities plus declared table counts. It does not alter legacy path evaluation,
CPU rasterization or compositing. AveMotion uses these IDs to build contiguous
immutable asset tables and typed references without relying on keypath strings
or traversal ordinals. Pixel and evaluated-scene goldens must remain unchanged.


## 0004 — Parsed-model property and timeline introspection

Part 7 adds narrow read-only accessors to Telegram's parsed model and temporal
interpolator. They expose static transform matrices, animated transform
components and authored cubic easing control points to AveMotion's direct
parsed-model bridge. It also records whether a `tm` field was actually
parsed, preventing unrelated `ExtraLayerData` users from appearing as false
time-remap properties. The parser, evaluator, CPU rasterizer and compositing
behavior are unchanged. Pixel, evaluated-scene and render-plan goldens must
remain byte-for-byte identical.


## 0005 — Parsed source Shape/paint binding

Part 11 carries canonical `SourceNodeId` metadata from Telegram's parsed model
into the runtime path and paint items. A draw node now reports the single source
Shape node, source paint node, path count and whether a modifier touched the
path. The legacy evaluator, final geometry, CPU rasterizer and compositing
remain unchanged. AveMotion uses the seam only as a characterization binding;
source geometry is accepted after point-for-point parity with Telegram's local
path.
