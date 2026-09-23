# Hardened Telegram TGS loading

## Scope

Part 22 adds a transport/container layer only. It does not alter Lottie parsing,
timeline semantics, geometry, rendering, scheduling or visual output.

```text
compressed .tgs bytes
-> AveMotion::Formats
-> validated UTF-8 JSON
-> existing Runtime::loadLottieJson()
```

A `.tgs` animated sticker is a gzip member whose uncompressed payload is a
Lottie JSON object. AveMotion parses the gzip container itself and feeds only
the validated JSON to the established runtime.

## Public targets

```text
AveMotion::Formats
AveMotion::Runtime  (publicly depends on Formats)
```

`AveMotion::Formats` is available when the SDK is built without either rlottie
reference implementation. It has no dependency on Win32, Direct2D or Qt.

## Public API

```cpp
formats::AssetFormat detectAssetFormat(span<byte>);
formats::TgsDecodeResult decodeTgs(span<byte>, limits);
formats::TgsDecodeResult decodeTgsFile(path, limits);
bool formats::isValidUtf8(string_view);
```

Runtime convenience methods:

```cpp
Runtime::loadTgs(bytes, debugName, limits);
Runtime::loadTgsFile(path, debugName, limits);
Runtime::loadAssetData(bytes, debugName, limits);
```

`loadAssetData()` recognizes only:

- a gzip magic header (`1F 8B`) as TGS;
- optional JSON whitespace followed by `{` as plain Lottie JSON.

Unknown input fails closed. It is not guessed from a file extension.

## Default limits

`TgsDecodeLimits::telegramSticker()`:

```text
max compressed bytes:  64 KiB
max JSON bytes:         2 MiB
max optional field:     4 KiB each
max expansion ratio:    128:1
validate FHCRC:          yes
require valid UTF-8:     yes
require JSON object:     yes
reject trailing data:    yes
```

The output limit is intentionally larger than Telegram's normal documents so a
valid 64 KiB sticker is not rejected merely because JSON is verbose. The ratio
limit remains an independent decompression-bomb guard.

Trusted applications may explicitly opt into
`relaxedApplicationAsset()` (1 MiB compressed, 16 MiB JSON, 256:1). No relaxed
mode is selected implicitly.

## Gzip validation sequence

```text
1. validate configured limits
2. enforce compressed byte limit
3. check ID1 / ID2 / CM / reserved FLG bits
4. parse MTIME and optional FEXTRA / FNAME / FCOMMENT / FHCRC
5. bound every optional field before scanning/copying
6. read trailer CRC32 and ISIZE
7. enforce JSON byte and expansion-ratio limits before allocation
8. inflate raw DEFLATE into an exactly sized buffer
9. require end-of-stream and complete compressed-input consumption
10. compare produced byte count with ISIZE
11. verify CRC32 of uncompressed JSON
12. validate UTF-8
13. require a top-level JSON object envelope
```

Concatenated gzip members and bytes between the DEFLATE end and the one accepted
trailer are rejected. AveMotion currently accepts exactly one TGS gzip member.

## Decompressor isolation

The implementation vendors the `miniz.h` snapshot already present in the
Samsung comparison tree. Only raw `tinfl` inflation and CRC-32 are used. ZIP,
stdio and zlib-wrapper APIs are disabled.

Historical miniz emits several low-level symbols independently of its export
macro. Part 22 prefixes every externally visible symbol in AveMotion's private
translation unit so it can be linked alongside Samsung rlottie's own copy
without duplicate definitions.

No miniz declaration enters an installed AveMotion header.

## Error model

Every rejected input returns a typed `TgsErrorCode`, byte offset and explanatory
message. Error families include:

- size/ratio/limit failures;
- malformed or truncated gzip headers;
- unsupported method or reserved flags;
- optional-field/FHCRC failures;
- corrupt/truncated/raw-DEFLATE failures;
- trailing compressed data;
- `ISIZE` mismatch;
- payload CRC mismatch;
- invalid UTF-8 or JSON envelope;
- file-size/open/read failures.

`Runtime` maps container failures to
`RuntimeErrorCode::ContainerDecodeFailed`; parser/model errors after successful
decoding retain the existing runtime error categories.

## Diagnostics

`runtime::DiagnosticsSnapshot` adds:

```text
tgsDecodeAttempts
tgsDecodesSucceeded
tgsDecodesFailed
tgsCompressedBytes
tgsJsonBytes
```

A successful decompression followed by an unavailable/offline parser counts as
a successful TGS decode and a failed asset load. This keeps transport and model
failures distinguishable.

## Tests

The standalone format tests cover:

- a deterministic dynamic-Huffman fixture;
- stored DEFLATE blocks;
- FEXTRA, FNAME, FCOMMENT and valid FHCRC;
- bad magic/method/flags/header CRC;
- truncation and optional-field limits;
- corrupt DEFLATE and trailing data;
- too-small and too-large `ISIZE`;
- payload CRC mismatch;
- compressed/output/ratio limits;
- invalid UTF-8 and non-object JSON;
- file failures;
- deterministic byte mutation smoke.

Runtime integration tests prove that TGS and the source JSON produce identical:

- metadata and source hash;
- Telegram canonical model (where the Telegram parsed-model seam is available);
- evaluated-scene fingerprint;
- CPU reference pixels.

The Win32 preview self-test loads the `.tgs` fixture and then exercises the
existing Player, Direct2D, resize, DPI and device-recreation path.

## Deliberate boundaries

Part 22 does not add:

- network loading;
- ZIP/Lottie archives with external files;
- multiple gzip members;
- background decompression service;
- disk cache;
- SVG import;
- `.avm` compilation;
- extra Lottie rendering features.
