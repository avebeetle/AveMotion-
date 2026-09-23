# Direct2D backend seam

The optional Windows target `AveMotion::Direct2D` is isolated from
`AveMotion::Core`, `AveMotion::Runtime`, and `AveMotion::Rendering`.

## Host-owned graphics contract

The host owns:

- D3D/D2D device creation;
- swap chain or final target;
- `BeginDraw` / `EndDraw`;
- final present;
- graphics-domain and generation identifiers.

The backend receives a session containing a host-provided
`ID2D1DeviceContext`. It may change the context transform while drawing but an
RAII guard restores the incoming transform on all success and error exits.

## Implemented path

- `MoveTo`, `LineTo`, cubic Bézier and close verbs;
- winding/even-odd fill rules;
- solid fill;
- dashless solid stroke;
- line caps, joins and miter;
- separate geometry, presentation and host transforms;
- effective opacity;
- geometry and stroke-style caches;
- asset-scoped static geometry reuse;
- bounded instance geometry replacement by revision;
- graphics-domain invalidation and lazy recreation.

Unsupported paths are skipped and counted rather than approximated incorrectly:

- gradients;
- images;
- dashed strokes;
- clips;
- masks/mattes;
- effects/offscreen groups.

## Cache identity

Static geometry:

```text
AssetHandle generation + source GeometryId + canonical content hash
```

Animated geometry:

```text
InstanceHandle generation + evaluated slot + geometry revision
```

The native cache slot key contains stable packed generation identities, so
handle reuse cannot attach a stale resource to a new object.

## Device/resource policy

- graphics-domain ID/generation changes clear backend-owned resources;
- factory identity changes clear geometry/stroke resources;
- the first domain/factory adoption performs one reset, not two;
- logical assets and evaluated snapshots survive device recreation;
- canonical local geometry is independent of destination DPI/size;
- future bitmap/command-list caches require additional scale/domain keys.

## Part 18 verification layers

### Portable contract

`Direct2DBackend.cpp` itself is compiled and executed against deterministic
fake COM/D2D interfaces. The contract verifies path construction, Fill/Stroke,
resource sharing, cache reuse, domain invalidation, transforms, error cleanup
and unsupported-feature skipping. This is not a reimplementation of backend
logic: only the Windows calls consumed by production code are modeled.

### Native WARP smoke

On Windows, a real D3D11 WARP device and Direct2D device/context render to a
BGRA texture. The test reads pixels back, validates opaque and premultiplied
alpha Fill, Stroke and transparency, exercises two graphics devices and writes
`avemotion-direct2d-warp.ppm`.

See `docs/WINDOWS_DIRECT2D_GATE.md` for the exact command and evidence boundary.

## Verification status

Locally proven in the current Linux environment:

- public header does not leak Windows headers into backend-neutral targets;
- production backend implementation compiles and executes via the deterministic
  contract under Clang, GCC and AddressSanitizer;
- all existing semantic/oracle suites remain green.

Prepared but not locally executable without Windows SDK:

- actual MSVC compilation against Windows SDK;
- WARP D3D11/Direct2D execution;
- native pixel readback artifact.

Run `scripts/run_part18_windows.cmd` on Windows or use the dedicated GitHub
Actions job to close the native gate.

## Part 19 native capture corpus

The production backend is additionally exercised against the pinned Telegram
CPU rasterizer across 75 real cases:

```text
animated Shape
Rectangle / Rounded Rectangle / Ellipse
Star / Polygon
Trim Path
Repeater content-group Fill / Stroke
```

The Windows WARP test renders at 96, 144 and 192 DPI, square and wide targets,
reads transparent premultiplied BGRA pixels back and records tolerant
cross-rasterizer metrics. Every unchanged plan is drawn twice; the repaint must
create or replace no native geometry resource.

See `docs/DIRECT2D_CAPTURE_CORPUS.md`.
