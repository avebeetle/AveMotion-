# AveMotion Part25C — Fresh-equivalent recording lifecycle

Status: approved for autonomous execution under the user's delegated design and
plan authority, 2026-09-23. Base: `45a21a4`.

## Intent and continuation

The user wants an efficient Windows animation module embedded in AveVoice,
with host-owned graphics and scheduling. The latest instruction is to continue
all stages without intermediate approval pauses; previous run deadlines no
longer apply. This document covers only the prerequisite reference lifecycle.
Persistent Runtime Instance/model integration follows in a separate small plan
once the lifecycle is proved. Completing this substage does not stop automation.

Part25A proved ordinary `renderTree()` reuse incorrect. Part25B removed a
redundant mapping-only tree and added active-state regressions. Neither stage
implemented persistent sampling. Do not repeat those investigations or disguise
per-frame reconstruction behind a new session name.

## Approaches

1. **Selected: retain topology, reset evaluation scratch to constructor state.**
   Before each recording sample, reset every layer/content object's mutable
   evaluation fields, including objects skipped by the upcoming traversal.
   Preserve allocated object identities, topology, parsed model, source IDs,
   binding lists and immutable resources. Evaluate with the original algorithms
   and publish non-consuming paths. This avoids a parallel projected layer tree.
2. Visitation epochs plus a constructor-baseline publication tree can also hide
   stale descendants, but duplicates publication ownership and still needs
   content resets for skipped repeater copies. Retain as a documented fallback
   only if a concrete reset limitation requires it; not part of implementation.
3. Reconstruct every item, or cache all frame/viewport scenes: rejected. The first
   merely relocates current costs; the second scales memory with timeline and
   dimensions and does not establish a reusable evaluation lifecycle.

The selected approach is a correctness hypothesis, not an already-proved speedup.
The original fresh ordinary API remains the differential oracle.

## Binding constraints

- Keep Runtime scene/model sampling fresh throughout Part25C; CPU sessions stay separate.
- Preserve ordinary rlottie API behavior, visual semantics, goldens, bridge traversal, source IDs and canonical metadata.
- Keep dependency commits, licenses/notices, Direct2D ownership, Player threading, feature coverage and public fallback policy unchanged.
- No dependencies or Windows settings are installed or changed; no new platform threads are introduced.
- Both pinned variants must pass full-field fresh-ordinary versus retained-recording parity before Runtime adoption.
- No approximate comparison, golden update, new whitelist entry or hidden per-frame Animation/item reconstruction is allowed.
- Same-instance operations remain serial; separate instances own all mutable recording state.
- Retain raw test, patch and measurement evidence; ordinary push only after tests and independent review.

## Private upstream seam

Add the same additive C++ methods to each pinned `rlottie::Animation`:

```cpp
bool enableRecordingLifecycle();
const LOTLayerNode *renderTreeForRecording(
    size_t frameNo, size_t width, size_t height) const;
```

These are private vendored integration seams, not installed AveMotion API.
Enabling is one-way and succeeds only before an ordinary tree/render/property
operation. Metadata queries do not consume pristine state. Re-enabling an
already-recording object succeeds without resetting anything. The pristine latch
must be consumed before asynchronous render dispatch, not when a worker begins.
Concurrent access to a single Animation is unsupported, as before.

`renderTreeForRecording` on an ordinary object, or ordinary `renderTree` on a
recording object, returns null without changing evaluation state. CPU sync/async
render and property overrides on a recording object throw `std::logic_error`
before touching the surface, evaluating a callback, allocating/scheduling a
render task, or invoking a scheduler. These failures are misuse of the additive
private seam; existing ordinary callers retain their behavior. The future
Runtime adapter never calls forbidden operations on a recording object.

### Guard compilation addendum

The Telegram target disables C++ exceptions. The additive misuse errors require
stack unwinding in `src/lottie/lottieanimation.cpp`, including its by-value
callback wrappers. Enable exceptions only for that translation unit (`/EHsc`
under MSVC, `-fexceptions` otherwise), with source options ordered after the
target's disabling flags. Keep evaluator/raster translation units and ordinary
algorithm branches unchanged. Guards throw before calling those no-EH paths.
Test destruction of a callback-owned resource on rejection and preserve the
effective compile command. Apply the same narrow requirement to Samsung only
if its actual flags require it; do not claim an unexecuted GCC/Clang build.

Recording accepts positive dimensions representable by the existing integer
viewport API. Invalid dimensions return null without publishing a stale tree;
the next valid sample must remain correct. Existing frame mapping/clamping is
reused exactly. A mapped global frame -1 at constructor-size/aspect parameters
has no usable fresh tree (Telegram null; Samsung's unbuilt C-tree dereference is
invalid). The recording API returns null for this fresh no-build sentinel rather
than inventing a result or reproducing undefined behavior. Nested local frame -1
is valid and must retain its original trim/cache semantics.

No repeat-frame cache is necessary initially: reset and evaluate every recording
sample. Never reuse an old publication after failure. Exported pointers are
borrowed until the next sample or session destruction; AveMotion deep-copies
before either. Already copied scenes must survive both.

## Reset and evaluation contract

Reset is an explicit operation on the existing item graph, not placement-new,
memcpy of nontrivial objects, or replacement of a layer/content/drawable. Virtual
reset traversal may follow immutable child ownership; a second registry is not
needed unless a measured or correctness reason is recorded.

Reset every layer, including unreached descendants, to frame=-1, combined alpha=0,
identity combined matrix and constructor dirty flags. Preserve parent pointers,
complex-content decisions, ordering, model pointers and assigned IDs. Use each
variant's original `visible()` predicate: Telegram has an exclusive out frame,
Samsung an inclusive out frame. Do not standardize them.

Reset composition last-frame/view/aspect cache to constructor values before its
original update, preserving the fresh no-build sentinel above. Reset exported
mask/clip scratch to empty path/zero alpha, preserve immutable mask count, order,
mode and model data. Reset owned C publication scalar/pointer/count fields that
original builders assign only conditionally, retaining arrays/capacities and
owned gradient storage without stale aliases or double-free. Empty export is
still a complete hierarchy, not pruning of inactive layers or masks.

Reset content recursively even beneath a repeater with zero visible copies.
Groups and repeaters retain their allocated copies, paint/trim bindings and
source IDs but restore constructor matrices/alpha/path scratch. Shapes retain
immutable authored caches only where that cache cannot change a fresh result;
reset temporary paths and dirty/final-cache flags so modifiers run from fresh
inputs. Initially clear both shape and mask local authored paths into retained
storage each sample: an empty/gapped animated PathData update can return without
writing output, so retaining the prior input would not match a fresh object.
Trims restore their constructor cache (including frame=-1 and segment
(0,0)), then run the original algorithm in the original order. Do not replace
wrapped/whole/individual trim behavior with a supposedly improved algorithm.

Before original paint setters, restore typed drawable style payload defaults
in place. Dirty flags alone do not defeat approximate value comparisons.
Preserve drawable type and allocated style object; avoid repeated `setType`
allocations. Preserve gradients/images/names/IDs with correct ownership. Reset
exported disabled stroke fields as well: the bridge observes their values even
when `enable` is false. Do not unconditionally assign authored values where a
fresh object would preserve constructor identity under an epsilon comparison.

For each reached paint, use an owned recording path derived from the original
undashed path. Invoke exactly the same value-returning `VDasher::dashed(path)`
algorithm once per publication; Samsung's out-parameter overload differs on
all-zero dash patterns. Never consume the source path or CPU dirty state. Keep
Telegram source-local paths undashed and preserve metadata flags/relationships.

## Raster work and lifetime isolation

Telegram mask/clip updates currently schedule rasterization even for tree-only
calls. Recording mode must update their paths/alpha but skip those two raster
submissions. Otherwise repeated resets could overwrite an in-flight task; merely
adding a dirty bit or waiting on arbitrary raster work is not the design.
Propagate a recording-only flag through the existing owner chain. Ordinary tree
and CPU behavior remain unchanged. Samsung recording performs no preprocess
and must likewise schedule no raster work. No reset destroys or reuses a pending
CPU raster object because the one-way pristine guard prevents such objects.

## Evidence and acceptance

The common test compares deep-copied `EvaluatedScene` with
`tests/support/ExactSceneComparison.hpp`, unchanged exclusions only. Expected
samples always come from a newly loaded ordinary Animation, never a newly
enabled recording Animation. Retained candidate samples cover ascending and
reverse full timelines, deterministic seeks/repeats, 128x128 and 96x160 viewport
changes, zero/invalid dimensions then recovery, and two separate recording
sessions evaluated concurrently. Metadata queries before enable remain allowed.

Fixture families include the 16 smoke-corpus JSON inputs (8 files directly in
`tests/corpus` and 8 directly in `tests/fixtures`) and nested dash/epsilon
fixtures, plus new self-authored regressions for skipped repeater copies and
negative child time. Exercise inactive/zero-alpha precomps, masks/mattes/clip,
complex/noncomplex layer publication, static and animated trims, individual
partial/whole/wrapped trims, tiny width/dash/transform/opacity changes, zero dash
patterns, repeat publication, and copied-scene lifetime. Keep new fixtures nested
so the 16-asset corpus remains unchanged. Source-local fields are compared too.

Guard tests use sentinel-filled CPU buffers and a callback counter. Forbidden
calls must fail before either changes; async must not enqueue a task. Explicit
ordinary CPU before/after parity and the existing characterization suites protect
the oracle. Invalid enable after ordinary sync, tree, async dispatch, and property
override is checked on separate objects.

Record functional RED on the retained ordinary implementation before production
changes, then GREEN on the recording API. Independently review each variant.
Full final MSVC/CTest: Telegram, Samsung, windows-msvc-win32-preview (capture,
WARP, device recreation), no-reference Direct2D and installed external consumer.
Samsung's exact pre-existing Polystar scene/plan golden failures remain visible;
they are not an all-green result and may not broaden the acceptance whitelist.
No TSan or zero-allocation claim follows from functional concurrency tests.

## Provenance and delivery

Telegram patch `patches/telegram/0006-avemotion-recording-lifecycle.patch` and
Samsung patch `patches/samsung/0002-avemotion-recording-lifecycle.patch` contain
only the new local changes against their pre-Part25C vendored trees. Verify each
patch can reproduce the changed files from base45a21a4, byte for byte. Update
`third_party/rlottie/UPSTREAM.json` source tree hash/file/byte counts and patch
lists; preserve original commit/archive values and all license text. Document
the opt-in behavior in patch READMEs, docs/THIRD_PARTY.md and the stage report.
This is not a new licensing decision or permission for binary redistribution.

### Repository byte-preservation addendum

The installed Git uses core.autocrlf=true and this repository has no attributes.
A read-only raw-blob audit found nine untouched vendored files whose working
bytes differ from the committed blobs through newline normalization (both
variants' vs2019 files and Telegram example/efl_animview.cpp). Therefore a Git
roundtrip cannot currently reproduce all bytes protected by UPSTREAM.json.
This predates Part25C; do not silently recompute source fingerprints to bless it.

Add repository-local `-text` rules for `/third_party/**`, `/patches/**`,
`/tests/corpus/**` and `/tests/compatibility/tgs/**`. Preserve existing working
bytes and re-index affected tracked files under those rules in a separate
reviewed commit. No Windows/global Git configuration is changed, no license
bytes or animation data are edited, and no history is rewritten. Test actual
Git add/checkout roundtrips with LF/CRLF fixtures in an isolated temporary mini
repository under core.autocrlf=true; do not merely grep the attributes file.
Before final delivery, reconstruct the protected committed snapshot and run
the unchanged vendor/corpus verifier there as well as in the live workspace.

The numbered lifecycle patches remain diffs of actual changed upstream code,
not an unexplained line-ending rewrite of otherwise untouched upstream files.
Report both logical patch reproduction and Git byte-roundtrip verification.

Once both variants pass and the final independent review is resolved, continue
directly to Part25D persistent Runtime integration. Preserve the current Release
baseline under `out/benchmarks/part25c/baseline/avemotion_corpus_lab.exe`, SHA256
`be9b98b42fd0f610665163da3c676316bc14725a6cdf862c87ee2208f418da69`.
It is the Part25B runtime with the common instrument, not the original cda415c
runtime. Compare compatible baselines explicitly and never mix timing claims.
