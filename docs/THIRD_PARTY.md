# Third-party provenance

| Component | Exact source | Current role | License location |
|---|---|---|---|
| Telegram `rlottie` | `TelegramMessenger/rlottie@67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d` | primary parser, complete evaluator, CPU oracle and private semantic-parity source | `LICENSES/`, vendored `licenses/` |
| Samsung `rlottie` | `Samsung/rlottie@2cab35db755b0e39df40b679969495e90d39c578` | comparison and security-hardening donor | vendored `licenses/` |
| miniz 2.2.0 | snapshot already present in the Samsung comparison tree | private bounded raw-DEFLATE and CRC implementation for TGS; all emitted symbols are AveMotion-prefixed | `LICENSES/UNLICENSE-miniz.txt`, embedded header notice |

Exact archive and post-patch source-tree SHA-256 values are stored in
`third_party/rlottie/UPSTREAM.json` and checked by `scripts/verify_vendor.py`.

Telegram changes are explicit:

- `0001-modern-toolchain-build-only.patch`;
- `0002-avemotion-local-geometry-metadata.patch`;
- `0003-avemotion-stable-source-ids.patch`;
- `0004-avemotion-parsed-model-introspection.patch`;
- `0005-avemotion-source-geometry-binding.patch`.

Patch 0004 supplies the read-only parsed-model/easing/property introspection used
by the standalone evaluator. Patch 0005 carries canonical source Shape/paint
identities and modifier metadata into the Telegram render-tree seam for Part 11. The independent `AveMotion::Evaluation` target does not link
Telegram. Samsung's local sanitizer-hardening edit remains under
`patches/samsung/`.

No upstream type is exported by the installed AveMotion public headers.

## Part 22 miniz isolation

AveMotion's `third_party/miniz/miniz.h` is compiled only into the private Formats
implementation. Archive, stdio and zlib-wrapper APIs are disabled. Historical
low-level symbols are explicitly prefixed because Samsung rlottie separately
links another copy of the same header. No miniz type or symbol appears in the
installed public API.
