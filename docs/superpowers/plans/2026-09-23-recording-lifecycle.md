# Fresh-equivalent Recording Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in retained recording lifecycle whose copied scenes exactly match fresh ordinary rlottie evaluation in both pinned variants.

**Architecture:** Retain the existing object graph, IDs, bindings and resources; recursively restore constructor evaluation state before each sample. Publish owned non-consuming dash paths and suppress recording-only raster submissions. Keep all production Runtime sampling fresh until the separate Part25D integration stage.

**Tech Stack:** C++20 AveMotion, pinned C++ rlottie, MSVC19.44, CMake/Ninja, existing Python verification scripts, Windows/Direct2D/WARP.

**Spec:** `docs/superpowers/specs/2026-09-23-recording-lifecycle-design.md`

## Global Constraints

- Keep Runtime scene/model sampling fresh throughout Part25C; CPU sessions stay separate.
- Preserve ordinary rlottie API behavior, visual semantics, goldens, bridge traversal, source IDs and canonical metadata.
- Keep dependency commits, licenses/notices, Direct2D ownership, Player threading, feature coverage and public fallback policy unchanged.
- No dependencies or Windows settings are installed or changed; no new platform threads are introduced.
- Both pinned variants must pass full-field fresh-ordinary versus retained-recording parity before Runtime adoption.
- No approximate comparison, golden update, new whitelist entry or hidden per-frame Animation/item reconstruction is allowed.
- Same-instance operations remain serial; separate instances own all mutable recording state.
- Retain raw test, patch and measurement evidence; ordinary push only after tests and independent review.

User explicitly chose main and delegated plan/design approval and ordinary push.
Do not create another worktree or wait for routine confirmation. One product
implementer at a time; root owns commits/push coordination and review. Workers
may commit their scoped task only, never push or stage controller documents.

## Review Focus

1. A CPU/async/property call crosses the recording boundary: reject before touching surface, invoking callback or submitting a task (Tasks1/2 guard tests).
2. Invisible ancestors or zero-copy repeaters hide descendants with prior state: reset all owned descendants, preserve full hierarchy and constructor visibility (Tasks1/2 fixtures and full comparator).
3. Tiny changes or disabled stroke fields preserve prior payload: reset constructor baselines and compare inactive fields as well (Tasks1/2 epsilon and zero-dash fixtures).
4. Failed/zero viewport sample or repeated publication returns old borrowed storage: reject, recover and preserve deep copies through next sample/destruction (Tasks1/2 lifetime tests).
5. Mask/clip raster work survives into the next reset: recording must never submit it; ordinary CPU remains unchanged (Tasks1/2 source/lifetime review, repeated mask stress; Task4 full graphics/CPU gates).

## File responsibility map

- Both `third_party/rlottie/<variant>/source/inc/rlottie.h`: additive private recording API contract.
- Both `src/lottie/lottieanimation.cpp`: mode/pristine guards and recording entry.
- Variant root `source/CMakeLists.txt` where needed: source options in the target declaration directory enable exceptions only for the guarded lottieanimation.cpp wrapper, leaving other target sources unchanged.
- Both `src/lottie/lottieitem.h` and `lottieitem.cpp`: graph reset and recording-only path/raster behavior.
- Samsung `src/lottie/lottieitem_capi.cpp`: non-consuming publication/reset.
- Variant `src/vector/vdrawable.h` only where needed: typed stroke/dash payload reset in place; no raster algorithm edit.
- `tests/recording_lifecycle_tests.cpp`: common guard/parity/lifetime/concurrency driver.
- `tests/support/RecordingLifecycleTestSupport.hpp`: fixture loading and ordinary/candidate bridge helpers, only if needed to keep driver focused.
- `tests/fixtures/reference_sessions/`: new adversarial JSONs only; no top-level corpus additions.
- `CMakeLists.txt`: private test target, existing internal include/link conventions.
- `patches/<variant>/`: numbered reproducible local patches and rationale.
- `third_party/rlottie/UPSTREAM.json`, `docs/THIRD_PARTY.md`: exact provenance.
- `docs/PART25C_RECORDING_LIFECYCLE_REPORT.md`: final gates and limitations, not an optimization claim.

## Common test pattern

Use the real private bridge (`src/runtime/RlottieSceneBridge.hpp`) to copy both
trees immediately, with equal source hash/frame/dimensions. Neither result is
promoted through Runtime/model canonicalization. `ExactSceneComparison` remains
unchanged. Every candidate sample uses the same retained Animation; every oracle
sample uses a newly loaded ordinary Animation. A fresh recording oracle would
hide systematic errors and is forbidden.

```cpp
auto expectedAnimation = rlottie::Animation::loadFromData(json, freshKey, {}, false);
auto expected = copyTree(expectedAnimation->renderTree(frame, width, height));
auto actual = copyTree(candidate->renderTreeForRecording(frame, width, height));
const auto mismatch = avemotion::test::ExactSceneComparison{}.difference(expected, actual);
require(mismatch.empty(), caseName + ": " + mismatch);
```

`copyTree` is a test helper calling `buildSceneFromRlottieTree` with the same
sourceHash, serial/frame and dimensions. Return a copied scene or fail with its
typed error; never retain raw tree pointers across a new sample. Reuse the
existing comparator, not a second implementation. Fixture JSON follows existing
schema/test infrastructure. Expected values for three existing epsilon cases
remain independently pinned in `reference_session_tests.cpp`.

### Task 1: Telegram recording lifecycle and common differential tests

**Status:** complete at bd2724d; independent task review approved. Detailed step
evidence is in the task-1 report/ledger. Full Telegram62/62; controller focused4/4.

**Files:** Telegram files in the map; new test/support/nested fixtures;
`CMakeLists.txt`; `patches/telegram/0006-avemotion-recording-lifecycle.patch`,
`patches/telegram/README.md`, `third_party/rlottie/UPSTREAM.json`,
`docs/THIRD_PARTY.md`. Do not edit Samsung or Runtime.

**Interfaces:** Consumes existing `rlottie::Animation::loadFromData`, ordinary
`renderTree`, private bridge and `ExactSceneComparison`. Produces exactly
`bool Animation::enableRecordingLifecycle()` and
`const LOTLayerNode *Animation::renderTreeForRecording(size_t,size_t,size_t) const`.
Test target `avemotion_recording_lifecycle_tests`, CTest
`avemotion.runtime.recording_lifecycle`, initially Telegram only.

- [ ] Read spec and `out/part25c-lifecycle/reset-audit.md`; integrate the concrete reset map. Read `out/part25c-skipped-probe/findings.md` when available for fixtures; absence does not authorize guessing their semantics. Root will relay any late evidence.
- [ ] Create the differential test first using an explicitly temporary candidate adapter that invokes retained ordinary `renderTree`. Cover existing nested dash and translation/width/opacity files, all16 smoke inputs (8 JSON files directly in tests/corpus plus8 directly in tests/fixtures), static/animated trims, invisible/zero-alpha descendants and viewport history. Run it and save functional RED with exact field mismatch under `out/part25c-telegram/`. This adapter is replaced by recording API after RED, not retained as a fallback.
- [ ] Add guard/lifetime tests before implementation. The exact guard assertions are:

```cpp
require(!usedOrdinary->enableRecordingLifecycle(), "ordinary use forbids mode switch");
require(recording->enableRecordingLifecycle(), "pristine enables recording");
require(recording->enableRecordingLifecycle(), "enabling is idempotent");
require(recording->renderTree(0, 128, 128) == nullptr, "wrong tree entry rejected");
require(ordinary->renderTreeForRecording(0, 128, 128) == nullptr, "ordinary is not recording");
require(recording->renderTreeForRecording(0, 0, 128) == nullptr, "zero width rejected");
// renderSync, render and setValue on recording each throw logic_error.
// Check literal sentinel pixels and zero callback invocations after each.
```

Use separate objects for ordinary sync, async dispatch, tree and property
override then failed enable. Join ordinary async work before destruction.
Metadata queries before enable succeed; invalid viewport followed by valid
sample matches fresh. Preserve copied scene, sample another frame/dimensions,
destroy candidate, then compare preserved copy to original oracle.

- [ ] Add the one-way mode and synchronous pristine latch in AnimationImpl. Guard async before scheduler/task mutation; guard all setValue overloads through their common implementation. Use explicit `std::logic_error` for forbidden raster/property calls. Keep normal code branches unchanged.
- [ ] Because Telegram disables exceptions target-wide, first pin callback-owner cleanup on guard rejection, then enable exceptions only for lottieanimation.cpp with source options (/EHsc or -fexceptions) after disabling target flags. Preserve effective compile commands, guard before entering no-EH evaluator/raster code, and report unexecuted cross-compiler coverage honestly.

```cpp
bool AnimationImpl::enableRecordingLifecycle() {
    if (mRecordingLifecycle) return true;
    if (mOrdinaryUsed) return false;
    mRecordingLifecycle = true;
    return true;
}
```

- [ ] Add recursive `resetForRecording()` methods on existing composition/layer/content owners. Retain topology and typed payload allocation. Every sample resets composition to constructor cache, all layers frame=-1/alpha0/identity/All, masks/clip output empty, all content including skipped repeater copies, trim cache and shape temporaries/frame caches. Rebuild authored geometry into retained storage initially; do not introduce an authored-frame cache optimization here.
- [ ] Restore existing drawable typed stroke/dash defaults before original paint setters. Reset conditional publication fields, including disabled stroke, without freeing retained gradient storage incorrectly. Preserve image ownership, names, IDs and stable binding data.
- [ ] Implement recording-only owned dash publication using the original value-returning dasher operation. Keep original undashed source/local paths untouched and ordinary sync unchanged. Propagate recording flag so Telegram mask/clip paths/alpha update but rasterize calls are skipped; no CPU preprocess call is added.
- [ ] Switch the candidate adapter to `enableRecordingLifecycle()` plus `renderTreeForRecording`. Run focused GREEN. Add regression fixtures for every new root cause before fixing it: outer-paint/trim/nested repeater zero/fractional copies, child local -1, negative visible-at-constructor child, zero dash patterns. Keep fixtures nested and validate16-asset corpus unchanged.
- [ ] Extend common test to ascending/reverse full timelines and deterministic seek/repeat sequence with square/non-square viewport changes for all fixtures. Add two-instance host-thread comparison and repeated mask/clip/destruction loops. Do not call functional concurrency a race-detector result.
- [ ] Update numbered patch and exact source hash/count/bytes using existing `scripts/verify_vendor.py` canonical fingerprint. Preserve archive hashes/commits/licenses. Reapply patch to base45a21a4 Telegram source in a new scratch directory and compare changed files byte for byte; do not alter old evidence.
- [ ] Configure/build focused target, run focused test, then full Telegram CTest once before scoped commit. All commands run under installed VsDevCmd:

```text
cmake --preset windows-msvc-telegram-debug
cmake --build --preset windows-msvc-telegram-debug --parallel 4
ctest --preset windows-msvc-telegram-debug -R avemotion.runtime.recording_lifecycle --output-on-failure
ctest --preset windows-msvc-telegram-debug --parallel 4 --no-tests=error --output-on-failure
python scripts/verify_vendor.py
git diff --check
```

- [ ] Self-review, commit only Task1 files as `feat: add isolated Telegram recording lifecycle`, report saved RED/GREEN/full output and exact remaining concerns. Root performs independent task review and ordinary push; no worker push.

### Task 2: Samsung recording lifecycle with the same oracle contract

**Status:** complete at0ebd660; independent task review approved. Full40/42 with
the two exact known Samsung baseline failures; controller focused4/4PASS.

**Files:** Samsung files in map; common test/support only for concrete variant
differences and added regression cases; `CMakeLists.txt`;
`patches/samsung/0002-avemotion-recording-lifecycle.patch`, Samsung README,
`third_party/rlottie/UPSTREAM.json`, `docs/THIRD_PARTY.md`. Telegram behavior stays
unchanged; no Runtime edits.

**Interfaces:** Consumes common test target/driver from Task1 and the exact two
recording API signatures. Produces Samsung's same signatures and enables the
common CTest target for both reference variants (not none).

- [ ] Read spec and reset audit's Samsung map. Capture functional RED by compiling common differential driver against Samsung with the temporary retained-ordinary candidate adapter; guard API absence is supplemental RED, not the sole proof. Save in `out/part25c-samsung/`.
- [ ] Add same mode/pristine/async/property guards and methods, using literal contract assertions from Common test pattern and Task1's committed guard tests without altering their expectations. Apply the actual Samsung constructor reset map, not Telegram member guesses: inclusive out-frame visibility, CApiData ownership, allocator-owned runtime graph, typed unique stroke/dash payloads and Samsung alpha convention.
- [ ] Recursively reset owned graph and C export scratch without reconstructing arena nodes or assigning nontrivial objects wholesale. Use original individual trim and value-returning dasher semantics, including all-zero patterns; never use the different out-parameter overload. No raster preprocess/scheduler call in recording.
- [ ] Run same fixture/access-order/viewport/lifetime/concurrency driver GREEN. Add a narrow RED fixture before any additional Samsung correction. Ordinary fresh oracle remains unchanged.
- [ ] Record reproducible numbered patch, source fingerprint/file/byte counts and docs; reapply patch to base45a21a4 Samsung snapshot and verify byte equality. Keep Telegram metadata intact.
- [ ] Configure/build full Samsung, focused recording test, full Samsung suite and focused Telegram test. Preserve/report the exact two known Samsung Polystar golden failures, compare their expected/actual hashes against Part25B baseline logs, and investigate every new failure.

```text
cmake --preset windows-msvc-samsung-debug
cmake --build --preset windows-msvc-samsung-debug --parallel 4
ctest --preset windows-msvc-samsung-debug -R avemotion.runtime.recording_lifecycle --output-on-failure
ctest --preset windows-msvc-samsung-debug --parallel 4 --no-tests=error --output-on-failure
ctest --preset windows-msvc-telegram-debug -R avemotion.runtime.recording_lifecycle --output-on-failure
python scripts/verify_vendor.py
git diff --check
```

- [ ] Self-review and scoped commit `feat: add isolated Samsung recording lifecycle`; report raw evidence and concerns. Root review/push follows, not worker push.

### Task 3: Preserve protected source bytes through Git checkout

**Status:** complete8748dae, independent review approved;642 working-byte hashes
unchanged,9 expected index corrections, snapshot verifierPASS, Telegram63/63.

**Files:** Create `.gitattributes`, `scripts/test_git_protected_bytes.py`;
modify `CMakeLists.txt` for a Python/Git-available test; re-index only byte
normalization differences under protected paths. Add rationale to
`docs/THIRD_PARTY.md`. Do not edit vendor working bytes, patch contents,
goldens, licenses or UPSTREAM.json fingerprint values.

**Interfaces:** Consumes the exact fingerprints after Tasks1/2 and current
working source bytes. Produces repository-local no-conversion rules, CTest
`avemotion.vendor.git_protected_bytes` where Git is available, and a real
committed-snapshot verification log. The test must run without requiring the
AveMotion source directory itself to be a Git checkout (source archives work).

- [ ] Write the behavior test before `.gitattributes`: create a mini Git repo
  via Python tempfile under the normal test temporary root; copy the repository
  attributes if present, then write literal LF and CRLF samples under all four
  protected path families. Invoke Git with command-local core.autocrlf=true,
  stage them, and `checkout-index --prefix=<separate-temp-output>/ --all`.
  Compare exact bytes to literals; no Git commit, user identity, network,
  global configuration, destructive cleanup of broad paths or source grep.
  Expected RED: LF-protected samples become CRLF without attributes.

```python
cases = {
    'third_party/probe/source.cpp': b'first\nsecond\n',
    'third_party/probe/project.sln': b'first\r\nsecond\r\n',
    'patches/probe/change.patch': b'first\nsecond\n',
    'tests/corpus/probe.json': b'{"value":1}\n',
    'tests/compatibility/tgs/SHA256SUMS.txt': b'hash  probe.tgs\n',
    'tests/compatibility/tgs/probe.tgs': b'\x00\x0a\x0d\xff',
}
```

- [ ] Capture RED command/output. Add repository `.gitattributes` with exact
  rules below and rerun GREEN. Cover autocrlf=false as a second roundtrip too.

```gitattributes
/third_party/** -text
/patches/** -text
/tests/corpus/** -text
/tests/compatibility/tgs/** -text
```

- [ ] Wire CTest only when existing Python and `find_package(Git QUIET)` both
  resolve; pass explicit Git executable/source attributes path. No Git
  repository assumption and no new installed dependency. Missing Git must not
  block building a source archive; report the unavailable optional check.
- [ ] Before re-index, save hashes of protected working bytes. Stage attributes
  and re-index affected tracked protected files under no-conversion rules.
  Confirm all working-byte hashes unchanged; only original normalized Git blob
  representations change. Expected untouched files are four vs2019 files per
  variant plus Telegram example/efl_animview.cpp; explain any additional file
  before staging it. Preserve unrelated controller edits.
- [ ] Run behavior test and existing vendor/corpus verifier. Reconstruct a full
  protected snapshot from the corrected index into a new scratch directory,
  with scripts/verify_vendor.py and required tests/corpus/SHA256SUMS; invoke
  unchanged verifier there. Save raw output. Keep existing numbered lifecycle
  patch artifacts unchanged because no working source bytes changed here.
- [ ] Run full Telegram CTest once, report all known warnings/failures honestly,
  self-review and commit scoped files as `build: preserve vendored bytes across Git checkouts`.
  Root independent review and ordinary push follow; no worker push.

### Task 4: Whole-lifecycle validation, provenance and next-stage handoff

**Status:** complete. Final full/Release/consumer gates at aec482f, independent
whole-change review45a21a4..3f8a06e approved with no blocking findings.
Telegram63/63,preview57/57,none30/30; Samsung41/43 with only the two unchanged
baseline Polystar failures. Runtime adoption continues in Part25D.

**Files:** `docs/PART25C_RECORDING_LIFECYCLE_REPORT.md`; controller STATE/ledger;
only concrete final-review corrections through their original worker. No new
optimization or feature is added in this task.

**Interfaces:** Consumes both proven APIs and common test. Produces durable gate
and ownership report for Part25D; no claim that Runtime sessions are persistent.

- [ ] Run fresh configure/build/full CTest on Telegram, Samsung, windows-msvc-win32-preview and windows-msvc-direct2d with preserved raw logs under `out/part25c-final/`. Use commands in Tasks1/2 substituting the preset; no bare shell lacking VsDevCmd.
- [ ] Run installed no-reference external find_package consumer configure/build/execute with a new stage-owned prefix. Preserve output and distinguish unavailable platform/sanitizer evidence from passing tests. Check common test absent for none.
- [ ] Verify vendor fingerprint, byte-reproducible patch application, unchanged licenses/goldens/16-asset corpus and unchanged Runtime/ReferenceRuntime production code against45a21a4. Run Release common recording test for both variants to expose optimization-sensitive comparisons; use new stage-specific Samsung Release build if needed, no dependency installation.
- [ ] Write report with API/reset ownership contracts, commands/counts, known Samsung baseline failures, remaining warnings, baseline executable hash, and limitations (no Runtime speedup or TSan/zero-allocation claim). Preserve scratch evidence.
- [ ] Root runs one independent whole-change review against base45a21a4, scoped fix review for any changes, fresh covering tests and ordinary push. Update STATE and immediately write/execute Part25D integration design/plan under delegated authority; keep avemotion-6 ACTIVE.

## Self-review and approval record

The spec's API, graph resets, raster isolation and parity map to Tasks1/2;
repository byte preservation maps to Task3 and final gates to Task4. Both
variants have independent review boundaries; shared test/manifest updates are
serialized. No placeholder implementation or
fallback to fresh object reconstruction is permitted. Controller approves this
written plan under explicit user delegation and chooses SDD. Prior Part25A/B
plans remain historical evidence, not redispatched tasks.
