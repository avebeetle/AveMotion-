# Part 25H — one-slot native emission feasibility

2026-09-23. Throwaway local experiment, not production implementation.

The candidate matched **1,008/1,008** fresh ordinary-reference scene/record
comparisons: 504 for `telegram_sticker_basic`, 504 for a controlled copy with
layer activity `[10,20)`. The controller read the harness and reran both cases
with distinct cache keys; both again passed, native exit 0.

Each case uses all frames 0..60 forward, 60..0 backward, then 0/60/0/60,
at 512x512, 256x256, 384x256 and 256x384. The existing exact comparator is
unchanged; model application is not performed. Its documented identity/history
exclusions still apply, including upstream change bits.

One preparation reference scene supplies immutable render IDs, source bindings,
layer layout and static opaque paint/flags. The candidate uses existing AveMotion
PropertyEvaluator and generateEllipsePath to calculate local/final geometry,
viewport placement, bounds, current visibility/draw omission, statistics and
fingerprints. Its emission function has no Animation, tree, reference callback
or per-frame scene cache. The reference animation used during preparation is
destroyed; each comparison uses a newly created ordinary reference animation.

Each variant reports 504 candidate emissions and 505 reference loads/samples
(one preparation plus 504 independent comparisons). These are harness counters,
not production diagnostics. No speed or allocation measurement was performed.

## Reproduction and limits

Local retained source/build/report: `out/part25h-native-proof/`. Commands:

```text
cmd /c out\part25h-native-proof\build.cmd
out\part25h-native-proof\proof.exe out\part25g-native-probe\normal.json proof-normal normal
out\part25h-native-proof\proof.exe out\part25g-native-probe\timed.json proof-timed timed
```

Final commands exited 0. Raw `normal.log`, `timed.log` and fresh
`controller-normal.log`, `controller-timed.log` preserve result counts.
`findings.md` discloses earlier scratch construction errors and learned fields.
Ignored scratch artifacts are local evidence, not installed/product deliverables.

The harness assumes known source positions and exactly one ellipse/fill slot.
It learns rather than independently derives static paint bytes and render IDs.
It is not admission for arbitrary inputs and proves neither model-applied
aliasing, planner/history parity, result retention, two-instance isolation,
opacity variants, multiple groups nor general transform semantics. It does not
make the runtime reference-free. No product source, public API, dependency,
golden, fallback or graphics ownership changed.

Next: Part25I private exhaustive raw-input gate, under its separately committed
spec/plan. Only after raw-to-model correspondence, identity mapping and broader
scene/plan/lifetime tests may a subsequent stage enable a native playback path.
