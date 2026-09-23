# AveMotion Part 18 report — Windows Direct2D validation gate

## Objective

Part 18 adds no new Lottie semantics. Its single objective is to turn the
existing Direct2D skeleton into a reproducible Windows production gate while
executing the same production implementation in a deterministic portable
contract test on every development platform.

Version:

```text
0.18.0
```

Release tag:

```text
stage17-windows-direct2d-gate-part18
```

## Baseline

Primary semantic oracle remains:

```text
TelegramMessenger/rlottie
67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d
```

Samsung remains comparison and hardening only:

```text
Samsung/rlottie
2cab35db755b0e39df40b679969495e90d39c578
```

Part 18 does not alter parser, evaluator, Shape, Trim Path or Repeater
semantics. All prior goldens remain unchanged.

## Implemented gate

```text
MotionRenderPlan
├── portable production-code contract
│   └── deterministic fake D2D/WRL objects
└── native Windows gate
    └── D3D11 WARP + real Direct2D + texture readback
```

### Portable contract

The actual `Direct2DBackend.cpp` is compiled with the same public AveMotion
headers and exercised against test-only interfaces that model only the Windows
calls used by the backend. The contract validates path translation, fill and
stroke state, transform composition/restoration, shared geometry, cache
behaviour, graphics-domain lifetime, error paths and fail-closed unsupported
features.

### Native WARP smoke

The Windows-only test creates a WARP D3D11 device, derives a Direct2D device and
context, renders three draw items, reads the BGRA target back to CPU memory and
asserts exact semantic regions. It then repeats the draw to verify cache reuse,
renders through a second device/domain, explicitly invalidates the domain and
writes a PPM reference image.

## Production fixes made during the gate

1. Included `avemotion/model/AssetModel.hpp` in the backend implementation.
2. Kept matrix composition in `D2D1::Matrix3x2F` rather than the base POD type.
3. Added an RAII transform guard so the host transform is restored on every
   return path.
4. Removed redundant first-domain reset/counting during factory adoption.
5. Added deterministic diagnostics assertions for cache and domain behaviour.

## Build and integration additions

- CMake option `AVEMOTION_BUILD_DIRECT2D_CONTRACT_TEST`;
- portable target `avemotion_backend_d2d_contract`;
- CTest `avemotion.direct2d.contract`;
- native CTest `avemotion.direct2d.smoke` using WARP;
- dedicated `windows-msvc-direct2d` configure/build/test presets;
- `scripts/run_part18_windows.ps1` and `.cmd`;
- dedicated GitHub Actions `windows-direct2d-warp` job;
- uploaded WARP image and CTest diagnostics in Windows CI;
- documentation of the host-owned graphics and evidence boundary.

## Local validation matrix

The environment used for local validation:

```text
Linux x86-64
CMake 3.31.6
Ninja 1.12.1
Clang 17
GCC 14.2
```

Results:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 31/31 |
| Telegram + GCC Release | 31/31 |
| Telegram + Clang AddressSanitizer | 31/31 |
| Samsung + GCC Release | 20/20 |
| Standalone/offline Clang | 11/11 |
| Installed package consumer | passed |

The Direct2D production contract passes under Clang, GCC and AddressSanitizer.
All prior Telegram/Samsung pixel, scene, model, property and render-plan gates
remain green.

## Native Windows status

The source tree contains a real Windows SDK WARP smoke and a dedicated MSVC/CI
path. The current Linux environment cannot execute MSVC, D3D11 WARP or
`ID2D1DeviceContext`, so the report does **not** claim that the native smoke was
run locally.

The native gate is performed by:

```powershell
scripts\run_part18_windows.cmd
```

Expected artifact:

```text
out/build/windows-msvc-direct2d/direct2d-artifacts/
avemotion-direct2d-warp.ppm
```

## Scope deliberately unchanged

Part 18 did not add:

- gradients;
- dashed strokes;
- clips, masks or mattes;
- effects or offscreen groups;
- chained Repeaters or Repeater+Trim;
- scheduler/player extraction;
- `.avm` serialization.

Unsupported backend features remain fail-closed and diagnostic.

## Next gate

Before adding another rendering feature family, run the native WARP gate on
Windows and retain the PPM/CTest evidence. After that, Part 19 should compare
Direct2D captures of representative proven corpus scenes against the Telegram
CPU pixel oracle and exercise target resize/DPI/resource recreation in a small
host preview.
