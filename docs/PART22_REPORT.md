# Part 22 report — hardened Telegram TGS loading

## Decision

Part 22 adds `.tgs` as a first-class, bounded input transport while preserving
the exact JSON parser/evaluator/rendering path already characterized against
Telegram.

```text
TGS -> validated JSON -> existing Lottie load
```

The implementation does not introduce a second parser and does not decompress
on the realtime/render path.

## Delivered components

- installable `AveMotion::Formats` target;
- public typed TGS API in `avemotion/formats/Tgs.hpp`;
- private raw-DEFLATE/CRC wrapper around vendored miniz;
- configurable strict limits;
- full gzip optional-header and trailer validation;
- UTF-8 and JSON-object-envelope checks;
- `Runtime::loadTgs`, `loadTgsFile` and auto-detecting `loadAssetData`;
- transport diagnostics;
- deterministic `.tgs` fixture corresponding to an existing JSON asset;
- malformed/limit/mutation tests;
- JSON-vs-TGS runtime, scene and CPU-pixel parity tests;
- Win32 preview `.tgs` loading and TGS-backed lifecycle self-test;
- Part 22 Windows runner and CI wiring;
- third-party provenance/license documentation.

## Security properties

Before allocating the output buffer, the decoder verifies the compressed size,
trailer-declared output size and expansion ratio. It accepts one bounded gzip
member, validates optional header fields and FHCRC, requires complete raw
DEFLATE consumption, then validates `ISIZE`, CRC-32, UTF-8 and the JSON object
envelope.

The default limits are 64 KiB compressed, 2 MiB JSON and 128:1 expansion.
Callers must explicitly request the relaxed trusted-application profile.

## Reference parity

For the deterministic fixture, plain JSON and TGS use identical decompressed
bytes and therefore produce identical source hashes. Telegram reference builds
also prove identical canonical model fingerprints, evaluated scene and CPU
pixels. Samsung builds prove transport/metadata/scene/pixel identity while
remaining conservative about the Telegram-only direct parsed-model seam.

## Build integration issue found

Samsung rlottie contains its own copy of the same historical miniz header. That
version emits low-level `tinfl`/`tdefl` symbols even when an export macro is
changed, so simply hiding visibility was insufficient and caused duplicate
static-link symbols.

AveMotion now prefixes every miniz symbol emitted by its private translation
unit. This preserves complete isolation and allows Telegram, Samsung and
offline builds to coexist without exposing or colliding with miniz APIs.

## Validation matrix

Completed in the Linux build environment:

```text
Clang offline/installable:       21/21 PASS
Telegram Clang Debug:            43/43 PASS
Telegram GCC Release:            43/43 PASS
Telegram Clang ASan:             43/43 PASS
Samsung GCC Release:             31/31 PASS
Samsung Clang ASan:              31/31 PASS
CMake install/export 0.22.0:     PASS
External find_package consumer:  PASS
Vendor/corpus verification:      PASS
```

The Windows-specific Part 22 gate is prepared in
`scripts/run_part22_windows.cmd`. It must still be executed on the user's
MSVC/Direct2D host because this build environment has no Windows SDK. The gate
loads the TGS fixture through the real Win32 preview, then reuses the already
proven WARP/Direct2D, DPI, device recreation and central-player checks.

## Deliberate exclusions

No network fetching, multiple gzip members, external-image archive format,
background loader, cache, SVG import, new Lottie semantic feature or `.avm`
format is introduced.
