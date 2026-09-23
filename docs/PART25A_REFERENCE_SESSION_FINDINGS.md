# Part 25A reference-session correctness gate

Date: 2026-09-23. Tested on Windows with MSVC 19.44.35229, Debug, both pinned
Telegram and Samsung rlottie variants. The probes did not change product or
vendor sources. This report rejects **unmodified persistent reuse**, not all
possible future implementations of persistent evaluation.

## Result

The existing fresh-tree-per-sample policy is necessary for the current exact
scene contract. Reusing `Animation::renderTree()` directly changes geometry and
retains inactive-subtree state. Both arbitrary seeks and complete ascending
frame scans fail, so a forward-only model-preparation scan is not exempt.

An isolated probe used the public rlottie API, AveMotion's existing scene
bridge and recorder. Each upstream tree was deep-copied before another call;
comparison identities were held equal. The fresh oracle used a separate
animation at the same frame and viewport. Fingerprint mismatches were followed
by checks of path coordinates, counts and layer/clip fields.

## Minimal reproduction

A static 100x100 rounded rectangle at (100,100), corner radius 20, stroke width
4, dash/gap 15/10 and offset 3, rendered at 200x200, changes immediately on the
second frame in both lineages:

| Frame 1 after frame 0 | Persistent | Fresh |
| --- | --- | --- |
| Scene fingerprint | `c66cec3ac4f6d651` | `8fad7fb88268955b` |
| Path verbs / points | 32 / 44 | 34 / 48 |
| Point 3 | (150, 103.9999161) | (150, 106.9999084) |

The probe also exercised the 16 existing JSON corpus/fixture files. Complete
ascending scans produced these first failing frames; each variant used its
own reported frame count:

| Asset | Telegram | Samsung |
| --- | ---: | ---: |
| Static dashed rectangle | 1 | 1 |
| `multi_trim_path_geometry.json` | 12 | 33 |
| `ModernPictogramsForLottie_LoudMute.json` | 56 | 39 |
| `1667-firework.json` | 61 | 31 |

The other 13 inputs matched the recorder fingerprints for these scans. That is
bounded test coverage, not a general eligibility proof. This probe did not
compare every downstream canonical-model field; it disproves the premise
that the model builder would receive the same evaluated scenes.

The seek sequence `0 -> middle -> middle -> last -> 0` additionally exposes
stale nested composition content. For Telegram LoudMute, returning to frame 0
retains six draw items where the fresh result has three. In firework, invisible
parent descendants retain clip paths and prior visibility.

## Why this happens

The historical report is
[`known-issues/RLOTTIE_RENDER_TREE_STATE.md`](known-issues/RLOTTIE_RENDER_TREE_STATE.md).
Current source inspection confirms multiple independent state transitions:

- Drawable tree synchronization expands dashes into the mutable source path.
  Repeated synchronization can dash a path that was already expanded. CPU
  preprocessing has a different consuming lifecycle and dirty-state reset.
- Inactive layer updates return before updating descendants. Tree publication
  still traverses descendants, exposing prior clips, draw lists and visibility.
  The exact-scene bridge and fingerprints intentionally record these fields.
- Multi-trim also differs without a dash pattern; its cache invalidation needs
  separate diagnosis. Correcting dash publication alone is insufficient.

Relevant pinned source locations before any vendor modification:
Telegram `src/lottie/lottieitem.cpp` (`LOTLayerItem::update`,
`LOTCompLayerItem::buildLayerNode`, `LOTDrawable::sync`) and
`src/vector/vdrawable.cpp` (`preprocess`); Samsung has corresponding logic in
`lottieitem.cpp`, `lottieitem_capi.cpp` and `vdrawable.cpp`.

Forcing a viewport change fixes some tested paths but not the inactive-subtree
case. Interleaving CPU rendering does not establish a reset boundary and caused
an `unknown upstream path verb` bridge failure on later samples in the probe.
Neither is an acceptable replacement for an ownership/lifecycle fix.

## Implementation consequence

Keep fresh exact samples and the separate lazy CPU session until a replacement
passes the full differential suite. A future recording lifecycle must preserve
pristine path data, correctly invalidate trim caches, and define fresh-equivalent
inactive-subtree publication without changing existing recorded semantics.

The eager `InstanceData::sceneAnimation` also supplies normalized-position
`frameAtPos` mapping; it is unused for exact sampling, not entirely unused.
Removing it would require its own mapping and creation-contract proof.

Session counters and benchmark instrumentation can land independently. No
performance claim follows from this investigation. If reuse remains deferred,
the final delivery must say so and report the measured baseline honestly.

## Bounded follow-up and execution decision

An independent scratch trim investigation isolated two cache invalidations:
the wholly included individual-trim branch leaves the previous partial final
path cached, and the wrapped/no-op branch can leave paint geometry cached.
Two scratch invalidations fixed this fixture's ascending and seek probes while
preserving its original fresh output. They are not a general evaluator fix and
were not applied to the vendor trees.

The recording-lifecycle feasibility assessment also found source-level risks
in approximate matrix/alpha/trim/stroke comparisons, static modifier scratch,
and skipped repeater contents referenced by outer operators. These additional
risks were inspected, not independently reproduced as new failing fixtures.
They prevent treating layer visitation plus owned dash output as a complete
solution. Telegram source-local geometry must match as well as final drawing.

A plausible future design retains topology and source IDs but gives each
sample fresh-equivalent evaluation scratch and a non-consuming, owned tree
publication. It needs explicit constructor-baseline behavior for inactive
layers, separate CPU ownership, and differential gates for both pinned
lineages, including sub-epsilon changes and zero-copy repeaters. This is a
proposal, not implemented or validated code.

For this bounded run, persistent scene and model reuse are deferred. The
remaining implementation is diagnostics, safe report generation, access-order /
viewport / CPU / two-instance tests, model retry coverage, and measured baseline
evidence. No vendor patch, dependency or license change is required by that
safe branch. The original allocation and speedup targets remain unmet.

## Local evidence

The ignored directory `out/part25a-probe/` contains `findings.md`, `probe.cpp`,
the static dash JSON, build recipes and both variants' seek/ascending logs.
Probe exit code zero means the comparison ran successfully, not that parity
passed; mismatches are explicitly recorded in its output. Product regression
tests added later must fail on a parity mismatch.

Additional ignored reports are `out/part25a-trim/findings.md` and
`out/part25a-recording-design/proposal.md`. Their exploratory estimates and
unproven lifecycle proposal must not be presented as performance evidence.
