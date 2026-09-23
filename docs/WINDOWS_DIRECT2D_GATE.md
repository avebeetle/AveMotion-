# Windows Direct2D validation gate

Part 18 adds a two-level validation gate for the Windows production backend
without expanding the supported Lottie feature set.

## Level 1: deterministic production-code contract

The normal production source file
`backends/direct2d/Direct2DBackend.cpp` is compiled on every supported host
against a small deterministic Direct2D/WRL contract shim. The shim is test-only;
it does not replace or enter the installed SDK.

The contract test executes the real backend implementation and verifies:

- `MoveTo`, `LineTo`, cubic Bézier and `Close` translation;
- winding/even-odd geometry construction;
- solid Fill and solid Stroke dispatch;
- cap, join, miter and stroke-width propagation;
- one native geometry shared by fill, stroke and Repeater-like copies;
- geometry and stroke-style cache hits on repaint;
- graphics-domain invalidation and lazy resource recreation;
- composition of geometry, presentation and host transforms;
- restoration of host transform on success and error paths;
- fail-closed skipping of unsupported feature bits;
- deterministic backend diagnostics.

This test caught three real integration defects before the native Windows gate:

1. the backend implementation used `CanonicalAssetModel` through an incomplete
   declaration but did not include its defining header;
2. the transform helper returned the base matrix POD rather than
   `D2D1::Matrix3x2F`, making matrix multiplication dependent on helper overload
   details;
3. the first graphics-domain adoption could clear/count the resource domain
   twice.

## Level 2: native Windows WARP smoke

On Windows, `avemotion_direct2d_smoke_tests` uses the actual Windows SDK:

```text
D3D11 WARP device
-> DXGI surface
-> ID2D1Device / ID2D1DeviceContext
-> AveMotion::Direct2D
-> BGRA target texture
-> staging readback
-> pixel assertions + PPM artifact
```

The smoke test validates:

- real `ID2D1PathGeometry` creation;
- real solid Fill and Stroke drawing;
- premultiplied half-opacity output;
- transparent background and stroke interior;
- native geometry reuse across three draw items;
- stroke-style reuse on repaint;
- a second graphics device/domain;
- explicit graphics-domain invalidation;
- creation of `avemotion-direct2d-warp.ppm`.

The test deliberately uses the software WARP device so it does not require a
discrete GPU or an interactive desktop session.

## One-command Windows run

From a Visual Studio developer shell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_part18_windows.ps1
```

or:

```bat
scripts\run_part18_windows.cmd
```

The dedicated preset is:

```text
windows-msvc-direct2d
```

It builds the standalone SDK and Direct2D backend without either `rlottie`
reference implementation, then runs the focused Direct2D tests and the full
standalone suite.

Expected image artifact:

```text
out/build/windows-msvc-direct2d/direct2d-artifacts/
avemotion-direct2d-warp.ppm
```

## CI

`.github/workflows/ci.yml` contains a dedicated `windows-direct2d-warp` job and
also retains Direct2D smoke coverage in the Telegram/Samsung Windows matrix.
The dedicated job uploads the WARP image and CTest log even when a later gate
fails.

## Current execution status

The current construction environment is Linux and has no Windows SDK. Therefore
Part 18 makes a strict distinction:

- **locally proven:** production backend source compiles and executes through
  the deterministic Direct2D contract, including cache/lifetime/error paths;
- **prepared but not locally executed:** the real Windows SDK, MSVC and WARP
  smoke executable;
- **required external gate:** run `windows-msvc-direct2d` on a Windows machine or
  GitHub Actions runner and preserve its image/log artifact.

No claim of a native Windows runtime pass is made until that external gate has
actually executed.
