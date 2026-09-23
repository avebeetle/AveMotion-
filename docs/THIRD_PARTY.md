# Third-party provenance

| Component | Exact source | Current role | License location |
|---|---|---|---|
| Telegram `rlottie` | `TelegramMessenger/rlottie@67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d` | primary parser, complete evaluator, CPU oracle and private semantic-parity source | `LICENSES/`, vendored `licenses/` |
| Samsung `rlottie` | `Samsung/rlottie@2cab35db755b0e39df40b679969495e90d39c578` | comparison and security-hardening donor | vendored `licenses/` |
| miniz 2.2.0 | snapshot already present in the Samsung comparison tree | private bounded raw-DEFLATE and CRC implementation for TGS; all emitted symbols are AveMotion-prefixed | `LICENSES/UNLICENSE-miniz.txt`, embedded header notice |

Exact archive and post-patch source-tree SHA-256 values are stored in
`third_party/rlottie/UPSTREAM.json` and checked by `scripts/verify_vendor.py`.
Repository `.gitattributes` disables line-ending conversion for vendored
sources, local patches, the smoke corpus, and the TGS compatibility corpus.
These rules preserve the exact bytes used by those fingerprints across Git
checkouts, including on systems with `core.autocrlf=true`; they do not change
upstream source, license, or corpus contents. The optional
`avemotion.vendor.git_protected_bytes` CTest checks a real add/checkout
roundtrip when Git is available.

Telegram changes are explicit:

- `0001-modern-toolchain-build-only.patch`;
- `0002-avemotion-local-geometry-metadata.patch`;
- `0003-avemotion-stable-source-ids.patch`;
- `0004-avemotion-parsed-model-introspection.patch`;
- `0005-avemotion-source-geometry-binding.patch`;
- `0006-avemotion-recording-lifecycle.patch`.

Patch 0004 supplies the read-only parsed-model/easing/property introspection used
by the standalone evaluator. Patch 0005 carries canonical source Shape/paint
identities and modifier metadata into the Telegram render-tree seam for Part 11. The independent `AveMotion::Evaluation` target does not link
Telegram. Samsung's local sanitizer-hardening edit and additive recording seam
remain under `patches/samsung/`.

No upstream type is exported by the installed AveMotion public headers.

Patch 0006 adds an opt-in, pristine-only recording lifecycle to the private
Telegram API. It retains the runtime topology while restoring evaluation scratch
before every sample, owns non-consuming dashed publication paths, and suppresses
recording-only mask/clip raster submissions. Forbidden raster/property operations
throw `std::logic_error`; exception unwinding is enabled only for the API
translation unit. Ordinary CPU rendering remains the oracle and production
Runtime still uses fresh ordinary sampling during Part25C. No dependency commit,
license text, or redistribution policy changes with this seam.

Samsung patch `0002-avemotion-recording-lifecycle.patch` adds the same private
opt-in API and guards, using Samsung's arena ownership, inclusive out-frame
visibility and original value-returning dash semantics. It retains typed stroke
payloads, gradient stop allocations and image brushes, clears shape/mask authored
scratch even for output-untouched PathData intervals, and performs no raster
preprocess. Only its API translation unit enables exception unwinding. Exact
fresh-ordinary versus retained-recording tests share the unchanged scene
comparator with Telegram. The two known Samsung Polystar scene/plan golden
failures are preserved; this seam does not change production Runtime sampling.

## Part 22 miniz isolation

AveMotion's `third_party/miniz/miniz.h` is compiled only into the private Formats
implementation. Archive, stdio and zlib-wrapper APIs are disabled. Historical
low-level symbols are explicitly prefixed because Samsung rlottie separately
links another copy of the same header. No miniz type or symbol appears in the
installed public API.
