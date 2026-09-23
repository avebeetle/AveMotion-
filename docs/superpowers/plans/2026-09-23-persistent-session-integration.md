# Persistent Session Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep one correct scene session per Instance and one temporary session per model scan, with stable Telegram source ownership across preparation and cache eviction.

**Architecture:** A private source-model lease removes cache identity ambiguity. Source metadata publication uses a mutex/epoch; unchanged recording frames consume only local bindings. Runtime then adopts recording lifetimes without sharing CPU state or changing host ownership.

**Tech Stack:** C++20 AveMotion, pinned rlottie, MSVC19.44/CMake/Ninja, existing Python tests, Windows Direct2D/WARP.

**Spec:** `docs/superpowers/specs/2026-09-23-persistent-session-integration-design.md`

## Global Constraints

- Keep ordinary rlottie raster/tree algorithms, goldens, bridge traversal and canonical model algorithms unchanged.
- Preserve public AveMotion API, feature coverage, error categories and fallback policy; no ANGLE, D3D9 or D3D11on12 work.
- Keep dependency commits, source licenses/notices, Direct2D ownership and Player threading unchanged.
- Same-instance operations remain serial; separate Instances own mutable evaluators and may run concurrently on host threads.
- No dependencies or Windows settings are installed or changed; no new production thread is introduced.
- Use full-field fresh-ordinary differential oracles; no epsilon, golden updates, comparator exclusions or hidden per-frame graph recreation.
- Preserve raw RED/GREEN, review, patch, platform and measurement evidence; ordinary push only after the corresponding tests and review.

Part25C full review/gates must finish first. User chose main and delegated written
approval/execution decisions. Controller selects SDD, one product worker, fresh
reviewer per task; workers commit scoped changes only, never push or stage
controller STATE/spec/plan. Source-ID cache-loss correction is explicitly
approved in spec, not a silent relaxation of metadata parity.

## Review Focus

1. Metadata stamping fails after partial writes: epoch must publish even on error/unwind, recording snapshot must match fresh ordinary metadata (Task1).
2. Different Assets share one source, but one assetMutex cannot serialize it: lock init/extraction consistently, no per-frame lock in unchanged epoch (Task1/2 stress and source review).
3. Cache eviction before/after prepare or Instance construction changes source object: exact Asset lease must be used for every Telegram session (Task2).
4. Viewport rejection or mid-scan evaluated overflow occurs after useful samples: no stale scene/partial model, one temporary session per retry (Task3).
5. Benchmark validator passes nonnegative but wrong session counts: explicit fresh/persistent profiles and deliberately corrupted output regression (Task3), common timing source hashes (Task4).

## File responsibilities and interfaces

- Telegram `source/src/lottie/avemotionanimationaccess.h`: private source lease/factory declaration.
- Telegram `source/inc/rlottie.h`, `src/lottie/lottieanimation.cpp`: friend/access definitions, guarded init and epoch snapshot.
- Telegram `src/lottie/lottiemodel.h`: binding metadata mutex/epoch only; no graph copy.
- Telegram `src/lottie/lottieitem.h/.cpp`: borrowed authored owner and conditional all-owner binding refresh.
- `src/model/TelegramParsedModelBuilder.hpp/.cpp`: source-handle overload and synchronized extraction/publication; canonical algorithms unchanged.
- `src/runtime/Runtime.cpp`: Asset source ownership, per-Instance scene session, local model session; public header only threading comments if needed.
- `tests/source_binding_tests.cpp`: direct vendor/parsed-builder binding/lease test, Telegram only.
- `tests/source_ownership_tests.cpp`: Runtime source eviction/shared-Asset regression, Telegram only.
- `tests/model_session_tests.cpp`, `tests/support/FreshModelOracle.hpp`: independent ordinary scan/application oracle, Telegram only.
- Existing runtime_seams/reference_session/frame_mapping/asset_model tests: exact new lifetime contract, no weakened semantic checks.
- `scripts/test_corpus_lab_output.py` and new validator regression: explicit profiles, common instrument unchanged.
- RootCMake: focused internal test wiring using current bridge/model/library conventions.
- Numbered Telegram patches0007/0008 as needed, manifest/THIRD_PARTY: exact bytes/provenance per task.
- `docs/EXTRACTION_SEAMS.md`, `docs/ASSET_MODEL.md`, final Part25D report: lifetime/threading/performance contract.

### Task 1: Private Telegram source lease, synchronized binding publication and recording refresh

**Files:** Telegram private seam/model/item/API files above, parsed-builder files,
new source_binding_tests.cpp, CMake, patch0007/provenance. No Runtime lifetime or
source-loading change yet. Samsung untouched.

**Interfaces:** Produce exactly:

```cpp
rlottie::AveMotionAnimationAccess::model(const rlottie::Animation&)
    -> std::shared_ptr<LOTModel>;
rlottie::AveMotionAnimationAccess::fromModel(const std::shared_ptr<LOTModel>&)
    -> std::unique_ptr<rlottie::Animation>;
avemotion::model::detail::buildTelegramParsedModel(
    const std::shared_ptr<LOTModel>& source,
    const AssetModelDescriptor& descriptor) -> ParsedModelBuildResult;
```

Keep existing JSON/key builder. Tests consume real recording API, ordinary tree,
bridge and unchanged ExactSceneComparison. CTest name avemotion.runtime.source_bindings.

- [ ] Read spec and out/part25d-design/source-binding-options.md. Audit all raw source-ID readers/writers once against current source; no blanket locking or eager full preparation.
- [ ] Add functional RED before production edits: cache-enabled unique JSON, retained recording animation constructed before existing JSON/key parsed builder, then fresh ordinary animation after builder; assert visible single-path paint authored IDs valid on oracle and compare full copied scenes. Capture stale-binding field mismatch. Supplement with handle/fromModel contract bodies before API implementation, not compile-only RED.

```cpp
auto retained = loadCached(uniqueJson, cacheKey);
require(retained->enableRecordingLifecycle(), "recording enable");
auto before = copy(retained->renderTreeForRecording(0,128,128));
auto parsed = buildTelegramParsedModel(uniqueJson, cacheKey, descriptor);
require(bool(parsed), parsed.error);
auto ordinary = loadCached(uniqueJson, cacheKey);
auto expected = copy(ordinary->renderTree(0,128,128));
require(liveAuthoredIds(expected), "oracle has applicable authored IDs");
same(expected, copy(retained->renderTreeForRecording(0,128,128)));
```

- [ ] Add private header/friend/factory using Animation's existing init. Return null for null source; metadata size/counts/fromModel ordinary pixels match loadFromData; model getter and clones share exactly the same pointer. No unsafe ownership cast.
- [ ] Add mutex/atomic epoch on LOTModel; init locks only composition construction and captures epoch. Add locked extraction helper and RAII epoch publication covering success/failure/exception exits; JSON/key/source overloads and property oracle all use it. Preserve all existing errors and table order.

```cpp
std::lock_guard lock(source->mAveMotionBindingMutex);
BindingPublication publication(*source); // destructor increments epoch, release
return Extractor{*source, descriptor}.build();
```

- [ ] Retain const LOTData* in path/paint bases through all8 typed constructors. Extend Telegram reset recursion with bool refreshBindings defaultfalse; copy raw IDs only when true, keep sequential IDs/path-count semantics unchanged.

```cpp
if (sourceEpoch.load(std::memory_order_acquire) != capturedEpoch) {
    std::lock_guard lock(sourceMutex);
    composition->resetForRecording(true);
    capturedEpoch = sourceEpoch.load(std::memory_order_relaxed);
} else {
    composition->resetForRecording(false);
}
// Existing update/build follows after unlocking.
```

- [ ] GREEN full copied-scene parity; cover rect/ellipse/shape/polystar and solid/gradient fill/stroke, hidden/repeater descendants and valid multi-path invalid-ID cases. Preserve before-scene immutability after refresh. For failure epoch test use a parsed input proven to stamp at least one ID before failing: assert changed epoch and retained-vs-fresh field equality after failure; save discovery evidence, do not invent a fixture or change builder acceptance.
- [ ] Concurrently extract same source under distinct descriptors while constructing ordinary/recording sessions; join and compare valid final snapshots, no shared Animation. Cover property-oracle epoch path. Source-review unchanged-epoch lock-free branch; no TSan claim from stress.
- [ ] Configure/build focused test, run source_bindings and recording_lifecycle, full Telegram once. Reapply new patch to pre-task Telegram source, verify all changed bytes, update exact source fingerprint/count/bytes and docs, git diff --check.
- [ ] Self-review scoped commit `fix: synchronize retained Telegram source bindings`; full report/red/green/raw paths to SDD task1 report. Controller review/push before Task2.

### Task 2: Asset-owned Telegram source identity independent of cache

**Files:** Runtime.cpp, source_ownership_tests.cpp, CMake, EXTRACTION_SEAMS/ASSET_MODEL lifetime notes. No scene/model reuse yet and no Samsung source edits.

**Interfaces:** Consume Task1 access adapter and source-handle builder. AssetData gains Telegram-only shared_ptr<LOTModel>; existing loadUpstreamAnimation signatures and public Runtime interface stay unchanged. CTest avemotion.runtime.source_ownership.

- [ ] Reproduce current Runtime cache-loss RED using out/part25d-cache-eviction/probe.cpp sequence in permanent test; use private LottieLoader capacity1 within isolated executable. Full comparator against explicitly prepared fresh ordinary model-aware oracle, not only invalid-ID counts. Cover old Instance and new Instance after eviction, eviction before preparation, and at least one live source-path/paint assertion.
- [ ] Capture model handle from successful metadata animation before its destruction. Store exact handle in AssetData. Telegram role loader uses fromModel; Samsung and metadata parse remain unchanged. Runtime parsed preparation calls handle overload; do not create extra metadata or reference sessions.

```cpp
#if AVEMOTION_TELEGRAM_PARSED_MODEL
assetData->sourceModel = rlottie::AveMotionAnimationAccess::model(*metadataAnimation);
// Subsequent role loader:
auto animation = rlottie::AveMotionAnimationAccess::fromModel(asset.sourceModel);
#else
auto animation = rlottie::Animation::loadFromData(asset.json, asset.cacheKey, {}, true);
#endif
```

- [ ] GREEN eviction semantics and full ordinary prepared-scene fields, model handles/aliases; test two separately loaded same-byte Assets with concurrent prepare and session creation, distinct final descriptor handles. Source lease survives cache disable/eviction but releases when all owners die; a direct weak_ptr ownership check may use private access, never public diagnostics hooks.
- [ ] Assert diagnostic epoch unchanged by lease capture: Metadata1; fresh scene sample1; fresh model sessions=N before Task3; lazyCPU1 repeated unchanged. CPU pixels before/after preparation/cache eviction remain identical for stable fixture.
- [ ] Run focused source_bindings/source_ownership/reference_sessions/model tests, full Telegram once and Samsung runtime focus. Update docs with deliberate metadata cache-loss correction, no claimed persistent lifetime yet. Scoped commit/review/push.

### Task 3: Persistent Instance and temporary model sessions with independent parity

**Files:** Runtime.cpp; runtime_seams_tests.cpp, reference_session_tests.cpp,
frame_mapping_tests.cpp, asset_model_tests.cpp; new model_session_tests.cpp and
FreshModelOracle.hpp; nested late-overflow fixture; scripts/test_corpus_lab_output.py
and scripts/test_corpus_lab_lifetime_profiles.py; CMake and lifetime docs.

**Interfaces:** InstanceData sceneAnimation replaces legacyFrameMappingAnimation;
helper consumes rlottie::Animation& explicitly. CTest avemotion.runtime.model_sessions
is Telegram-only; avemotion.corpus.lifetime_profiles validates both report profiles.

- [ ] Before production edits, update/add exact counter assertions to new contract and run RED. Newly created Instance Scene1; later exact samples Scene0 new; quiescent reset leaves live session creations0; prepared model Model1/SamplesN; repeat no work; CPU remains separate.
- [ ] Add independent fresh ordinary model oracle per out/part25c-lifecycle/model-scan-oracle-audit.md using unchanged parsed/update/finalize algorithms and same descriptor. Compare complete models by attaching to model-applied scenes with ExactSceneComparison; assert assetHandle separately, valid IDs and live static aliases/owner_before. Candidate model scan alone must change; oracle never calls Runtime model preparation to produce expected data.
- [ ] Add cold unique JSON load/createInstance/exactSample/prepare/modelSample order, and createInstance/prepare/modelSample without first exact sample. Require correct IDs and complete fields after source refresh, no second scene session. Include nested epsilon/dash/masks/repeaters/trim histories and viewport changes; retain existing concurrency/CPU checks.
- [ ] Add confirmed finite overflow fixture from out/part25d-late-failure/probe.cpp under nested reference_sessions only. Assert frame0 succeeds, frame1 typed error matches ordinary, frame0 recovers; failed prepare twice publishes no model and totalsModel2/Samples4/Failures2. Preserve pre-session overlimit/unsupported rejection0 and same-Asset concurrent single-flight success1.
- [ ] Add corpus validator explicit `--lifetime-profile fresh|persistent`, defaultpersistent; retain `--expected-setup-scene-sessions` override for original/Part25B baselines (default derived persistent1/fresh0). Require first/steady scene sessions fresh1/samples versus persistent0/0, model sessions fresh=samples versus persistent1. A behavior test copies real report data into temporary dirs and corrupts one count at a time; validator must reject the wrong profile/counts before GREEN. Timing instrument stays byte-identical.
- [ ] Implement eager create/enable scene session before publishing Instance and one local model session after successful guards/parsed extraction. Replace helper's internal load with supplied recording reference; pass each owner explicitly. Preserve clamp, viewport and error/counter/evaluationSequence order; use same scene object for legacy huge-frame mapping. No hidden recreation/fallback.

```cpp
auto scan = loadUpstreamAnimation(asset, ReferenceSessionRole::ModelPreparation);
if (!scan || !scan->enableRecordingLifecycle()) {
    asset.modelError = "unable to create a recording model-preparation runtime tree";
    asset.runtimeState->assetModelBuildsFailed.fetch_add(1U, std::memory_order_relaxed);
    return {nullptr, asset.modelError};
}
for (std::size_t frame=0; frame<asset.metadata.totalFrames; ++frame) {
    auto sampled = extractExactScene(*scan, asset, asset.handle, {}, 0U,
                                    frame+1U, frame, width, height,
                                    ReferenceSampleRole::ModelPreparation);
    // Existing update/error/finalize/publication remains in original order.
}
```

The new setup failure uses the existing AssetModelResult/counter return path;
all downstream existing validation/sample error messages remain unchanged.
Role still controls sample count, never chooses session ownership.

- [ ] GREEN focused all lifetime/parity/validator tests, then full Telegram and Samsung once, with exact two baseline Samsung failures identified. Run focused Release integration tests both variants, vendor integrity and diff check. Update docs/public threading comments: Instance serial, Asset model publication safe, shared authored lease only, unchanged-epoch frame has no source lock. Scoped commit/review/push.

### Task 4: Full platform gates, controlled measurements and independent handoff

**Files:** docs/PART25D_PERSISTENT_SESSIONS_REPORT.md and controller STATE/ledger;
no opportunistic product feature work.

**Interfaces:** Consume reviewed Task1–3 commits; produce exact SHA/test/benchmark
record and explicitly measured performance acceptance, not a speedup promise.

- [ ] Run fresh configure/build/fullCTest Telegram, Samsung, explicit win32-preview, none/Direct2D, installed consumer. No reference test present for none. Run Release direct lifecycle/binding/ownership/model-session gates both variants where applicable. Preserve exact known Samsung baseline rows; investigate every new failure.
- [ ] Check source patches reapply, vendor source fingerprints, protected Git checkout test, licenses/goldens/corpus unchanged. Record exact final candidate binary hash and sameinstrument source hashes.
- [ ] With workers/builds/tests stopped, run out/benchmarks/part25c/run-persistent-comparison.ps1 for Original then Part25B baselines using verified candidate SHA. Inspect raw outputs and validate fresh/persistent report profiles. Script is scratch/syntax-checked only until executed; independently verify counters and16 byte-identical semantic manifests.
- [ ] Summarize each asset's exact/pipeline median/p95 for A1/B1/B2/A2, observed setup/steady counts, actual evaluator/projector storage counters and1/16/64 current/peak memory. Compare2x/noassetp95>10% original target, report failures/tradeoffs honestly. No planner zero-allocation, race-detector or private-corpus performance claim.
- [ ] Final independent whole-change review receives stage-base diff, reports and deferred-minor/rulings ledger. One fix wave/scoped review if needed, then fresh covering/full gates and ordinary push. Preserve SDD/raw evidence. Continue only next separately designed in-scope stages; no unsupported licensing/fallback/API expansion.

## Self-review and delegated approval

Foundation precedes source ownership; source ownership precedes persistent scan.
Shared CMake/provenance/runtime files are serialized. Independent model oracle,
eviction, failure/unwind epoch, invalid dimensions and strict report validation
each have an owner. Full graphics/Release/consumer gates are explicit. No timing
assertion enters CTest. Controller approves this plan and SDD under delegated
authority; execution remains gated on Part25C completion, not ordinary user input.
