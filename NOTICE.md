# Notices

AveMotion Extraction Laboratory Part 24 contains:

1. AveMotion-owned core, immutable canonical model, host-driven playback,
   independent property evaluator, render planning, headless recording,
   diagnostics, tools and tests whose final public license has not yet been
   selected;
2. a Windows-only AveMotion Direct2D backend seam whose public contract is
   isolated from the backend-neutral core;
3. a vendored TelegramMessenger/rlottie snapshot at commit
   `67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d`, used as the primary parser,
   complete evaluator, evaluated-scene, local-path and CPU-pixel baseline;
4. a vendored Samsung/rlottie snapshot at commit
   `2cab35db755b0e39df40b679969495e90d39c578`, used only for comparison and
   selected hardening;
5. unmodified third-party license and copyright notices inside both source
   trees;
6. five documented Telegram patches: modern-toolchain/build integration,
   local-geometry metadata, stable source IDs, a read-only parsed-model/property
   introspection seam and source-geometry binding metadata;
7. one documented Samsung sanitizer-hardening patch;
8. private Telegram exact-property and local-path oracles used only by reference
   tests and characterization; installable AveMotion core/evaluation/rendering
   targets do not export upstream types;
9. AveMotion-owned Rectangle, Ellipse, Polystar and Polygon generators that
   intentionally reproduce pinned Telegram constants and operation order and
   remain guarded by exact local-path parity tests;
10. a backend-neutral Trim Path implementation adapted from observable
    behaviour and algorithms of LGPL-covered `VPathMesure`, `VDasher`,
    `VBezier`, `VLine` and `LOTTrimData`. Its source file preserves an explicit
    LGPL-2.1-or-later derivative-work notice and must remain subject to the
    applicable obligations;
11. AveMotion-owned Repeater transform/opacity evaluation that intentionally
    reproduces the pinned Telegram `LOTRepeaterTransform`, `LOTRepeaterItem` and
    `VMatrix` operation order and numerical behaviour, guarded by matrix,
    opacity and final-path parity tests;
12. dedicated one-/multi-path Trim and 61-frame Repeater fixtures with exact
    Telegram parity oracles;
13. retained AveMotion projection/trim/repeater workspaces whose capacity
    stability is verified after explicit preparation;
14. a native Windows/MSVC/Direct2D preview and 75-case WARP capture corpus;
15. characterization-only raw and portable source-geometry fingerprints. The
    portable identity quantizes projected path coordinates to 1/64 asset unit
    and is used only to confirm low-order MSVC/libm equivalence while runtime
    revisions, cache keys and rendering remain exact;
16. an AveMotion-owned application-independent centralized player/scheduler
    with stable generation handles, host-provided frame/wakeup callbacks,
    visibility policies and bounded due-frame storage. It creates no hidden
    thread, timer or bitmap frame ring;
17. a private `miniz.c 2.2.0` snapshot under the public-domain/Unlicense notice,
    used only for bounded raw-DEFLATE and CRC processing of Telegram `.tgs`
    containers; its emitted symbols are AveMotion-prefixed to coexist with the
    independent Samsung rlottie copy;
18. an AveMotion-owned hardened TGS container layer with configurable byte and
    expansion limits, gzip optional-header/trailer validation, strict UTF-8 and
    JSON-object checks, typed errors, transport diagnostics and JSON/TGS parity
    tests;
19. an AveMotion-owned backend-neutral canonical asset validator with structural
    graph/table/value checks, typed feature inventory, application, Direct2D-native
    and Telegram-sticker profiles, deterministic reports and an installable public
    API that exports no rlottie, Win32 or decompressor types;
20. a deterministic generated TGS compatibility corpus derived from versioned
    project fixtures. Its canonical gzip envelope uses stored DEFLATE blocks for
    byte-identical regeneration across host zlib versions; it is not represented
    as a downloaded real-world Telegram sticker pack.
21. an AveMotion-owned private/external corpus laboratory that scans `.tgs`
    and Lottie JSON without copying source assets, emits content-hash aliases by
    default, measures load/model and current native pipeline costs, estimates
    retained instance storage and produces a deterministic feature-priority
    report. The committed deterministic corpus is a smoke test, not a claim of
    real-world Telegram sticker representativeness.

The Telegram lineage is LGPL-2.1-or-later at its core and includes separately
licensed bundled components. The supplied Samsung snapshot identifies its core
under MIT and also includes separately licensed bundled components. Consult the
root `COPYING` and `licenses/` directories inside each source snapshot.

The miniz snapshot identifies itself as public domain / Unlicense; its embedded
notice and the copied Unlicense text are retained.

Do not redistribute this laboratory as a public SDK until the licensing model
for AveMotion-owned code and all derivative portions has received a file-level
review.
