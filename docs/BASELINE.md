# Current baseline decision — Part 19

## Primary semantic lineage

```text
TelegramMessenger/rlottie
67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d
```

Telegram remains the primary behaviour oracle for parsing, evaluation,
geometry modifiers and CPU reference rendering.

## Comparison lineage

```text
Samsung/rlottie
2cab35db755b0e39df40b679969495e90d39c578
```

Samsung remains a comparison/security-hardening donor. Behavioural differences
are never accepted automatically.

## AveMotion-owned path

```text
canonical model
-> property / spatial / world / Shape evaluation
-> source Shape / primitive / Trim / Repeater projection
-> MotionRenderPlan
-> headless or Direct2D backend
```

Unsupported features fail closed to the Telegram oracle path.

## Windows gate status

Part 18.1 is closed:

```text
Visual Studio 2022 / MSVC build: PASS
native D3D11 WARP + Direct2D smoke: PASS
full standalone suite: 13/13 PASS
vendor integrity/order tests: PASS
```

Part 19 adds the next native gate without adding new Lottie semantics:

```text
75 Telegram CPU reference frames
<->
75 real Direct2D WARP captures
at multiple sizes and 96/144/192 DPI
```

The cross-rasterizer comparison uses documented tolerant metrics rather than
requiring identical antialiasing. Every failed case retains Telegram, Direct2D
and amplified-difference BMP evidence.

## Deferred

- gradients and dashed strokes;
- clips, masks, mattes, effects and offscreen groups;
- scheduler/player extraction;
- `.avm` compiler;
- AveVoice adapter.
