# AveMotion Part25D — Persistent sessions with stable source ownership

Status: written design approved by the controller under the user's explicit
delegation,2026-09-23. Execution starts only after Part25C final gates/review.
This is a continuation, not permission to repeat completed Part25A/B/C tasks.

## Intent and success

AveMotion is a Windows animation module for a host-owned AveVoice interface.
Remove per-frame reference graph construction without changing rendered output,
canonical model contents, public capabilities or host graphics/scheduling.
One eager recording Animation belongs to each Instance; one temporary recording
Animation serves each eligible model preparation attempt; lazy CPU Animation
remains separate and ordinary. The immutable authored source must remain the
same source throughout an Asset's lifetime, regardless of upstream cache churn.

The user delegated specification/plan approval, method selection, reversible
technical choices and normal main commits/pushes, and requested uninterrupted
stages. No routine confirmation is pending. No dependency installation, system
setting change, new license decision, external publication or destructive
operation is included. Do not stop automation after an intermediate task.

## Evidence and alternatives

Part25C establishes fresh-equivalent reset/publication for both vendors while
Runtime stays fresh. It is a prerequisite, not proof of correct integration.
Source audit and scratch probes additionally establish:

- Eight Telegram path/paint constructors snapshot mutable source IDs before
  parsed-model extraction stamps them. Retained sessions cannot see later IDs.
- Cache capacity defaults to10; eviction and concurrent misses can produce
  distinct LOTModels for the same key. A retained session can own model A while
  the parsed builder stamps model B. A shared key is not an ownership contract.
- Current fresh Runtime already loses source bindings after eviction: a
  prepared StickAndBall scene goes from3 valid path/paint IDs to0/0 while still
  reporting its model applied. Evidence:out/part25d-cache-eviction/findings.md.
- Different Assets can share one cached source, but their modelMutex objects
  are different. Raw ID writers and constructor readers need synchronization.

Selected approach: retain an exact parsed-source lease per Telegram Asset,
construct its sessions from that lease, and refresh local recording bindings
only after a source metadata epoch changes. This retains one scene session and
does not require eager whole-model preparation or a lock on unchanged frames.

Rejected alternatives: (1) rebuilding an Instance after preparation violates
the one-session target and still needs eviction/locking treatment; (2) preparing
parsed data during load changes cost/failure timing and cold-source metadata;
(3) refreshing raw IDs alone leaves eviction and races unresolved. Do not add
whole-frame caches or a new shared mutable evaluator.

## Binding constraints

- Keep ordinary rlottie raster/tree algorithms, goldens, bridge traversal and canonical model algorithms unchanged.
- Preserve public AveMotion API, feature coverage, error categories and fallback policy; no ANGLE, D3D9 or D3D11on12 work.
- Keep dependency commits, source licenses/notices, Direct2D ownership and Player threading unchanged.
- Same-instance operations remain serial; separate Instances own mutable evaluators and may run concurrently on host threads.
- No dependencies or Windows settings are installed or changed; no new production thread is introduced.
- Use full-field fresh-ordinary differential oracles; no epsilon, golden updates, comparator exclusions or hidden per-frame graph recreation.
- Preserve raw RED/GREEN, review, patch, platform and measurement evidence; ordinary push only after the corresponding tests and review.

### Explicit source-identity compatibility amendment

Retaining a source lease intentionally removes cache-eviction-induced loss of
authored IDs. That accidental metadata loss is not preserved. Visual algorithms,
sequential render IDs, source table order and normal prepared-scene metadata
remain identical. An oracle after cache eviction must explicitly prepare its
own newly loaded ordinary source before comparison; an unstamped oracle is not
equivalent to a prepared Asset. Never exclude authored IDs from comparison.

Exact evaluation does not prepare models. A cold unique source starts with
invalid authored IDs; a cached source can already have IDs from another Asset.
After publication, the next recording sample snapshots current IDs. Old copied
scenes remain immutable. Model-aware evaluation completes its own prepareModel
before sampling, so applicable bindings must be valid and resolve against its
own immutable canonical tables. Multi-path/empty/modified cases retain their
existing deliberate invalidity and sourcePathCount semantics.

## Telegram private source ownership seam

Add a private vendor header `src/lottie/avemotionanimationaccess.h`, not an
installed AveMotion header. Forward-declare global LOTModel; declare a friend
`rlottie::AveMotionAnimationAccess` in Animation and define its methods beside
AnimationImpl, using the same constructor/init as ordinary loadFromData:

```cpp
struct LOT_EXPORT AveMotionAnimationAccess final {
    static std::shared_ptr<LOTModel> model(const Animation& animation);
    static std::unique_ptr<Animation> fromModel(const std::shared_ptr<LOTModel>& model);
};
```

Null input or a null root returns null from fromModel without creating an
Animation. Ordinary metadata/CPU behavior is unchanged. No raw
pointer ownership, void-pointer casts or public Runtime model handle is added.

Capture this lease from the existing successful metadata Animation in
loadLottieJson and retain it in Telegram-only AssetData. Metadata construction
and counters remain one. Subsequent Telegram scene/model/CPU loads use
fromModel; success increments exactly the same role counter as before. AssetData
holds no Animation, composition or mutable evaluator. Samsung keeps its
existing loader because it does not expose Telegram parsed source bindings.

Add private builder overload
`buildTelegramParsedModel(const std::shared_ptr<LOTModel>&, const AssetModelDescriptor&)`.
Runtime uses it. Keep the JSON/key overload for independent oracle callers and
delegate extraction through the same synchronization. The property-oracle
entry also runs Extractor and must follow that protocol. Descriptor-specific
MotionAssetModels are never shared through LOTModel: Assets retain their own
handle/debugName/final model, even when sharing the source lease.

## Source metadata synchronization and refresh

LOTModel owns a mutable binding mutex and atomic uint64_t epoch initially0.
Lock order is Asset modelMutex -> source binding mutex. Vendor code never
acquires Asset mutex; loader cache lock has been released before either path.

Hold source mutex during each complete Extractor build. Publish epoch with
release ordering on every exit that may have stamped IDs, including failure
and exception unwinding, before unlocking. A narrow RAII publication guard
preserves current partial-stamping failure behavior; no transactional metadata
policy is silently introduced. Incrementing on an extraction that stamped
nothing is acceptable. Property-oracle extraction uses the same discipline.

AnimationImpl::init holds source mutex while constructing the composition and
capturing epoch, protecting all eight constructor reads. Path/paint bases retain
one borrowed const LOTData pointer passed by the existing four path/four paint
constructors; lifetime comes from the Animation's source lease. Sequential
layer/node/geometry/paint assignment is not rerun during refresh.

At a valid recording sample, acquire-load epoch. If unchanged, perform the
existing all-owner reset using only local source-ID copies, without locking or
reading raw LOTData IDs. If changed, lock, refresh every path/paint copy during
the reset traversal (including hidden/repeater-owned descendants), capture
current epoch, unlock, then evaluate/publish. A bool refreshBindings parameter
with defaultfalse may extend Telegram reset methods; propagate it completely.
No lock spans normal evaluation, raster work, bridge copying or model scanning.

A reader observing an old epoch during concurrent stamping can use its previous
coherent local snapshot; it never reads fields being written. Its next sample
observes the completed publication. Model-aware own preparation completes before
its sample, so that sample cannot miss its successful publication. Functional
stress is required; unavailable TSan must remain explicitly unverified.

## Runtime session integration

After the source foundation and direct recording/model parity gates:

- Replace legacyFrameMappingAnimation with eager sceneAnimation in InstanceData.
  Enable recording before publishing Instance. Missing/rejected creation is
  InstanceCreationFailed; no partially usable Instance or hot-path replacement.
- Normal frame mapping keeps the proved Part25B metadata formula. The uncommon
  totalFrames>LONG_MAX case queries the same recording Animation's frameAtPos;
  no second mapping session. Metadata calls do not consume recording pristine.
- extractExactScene accepts an already-owned Animation reference. Keep viewport
  checks, clamp, sample counters, immediate deep-copy, typed errors and handles.
  Call renderTreeForRecording, never ordinary tree on a recording session.
- prepareStableAssetModel retains modelMutex, existing capability/timeline guard,
  parsed extraction, ascending update/finalize order and success-only publication.
  After successful parsed extraction create/enable one local recording session;
  reuse it for the entire scan, then destroy it on success or failure.
- CPU remains lazy ordinary, independent from recording. Source lease shares
  authored data, not raster/evaluation state. Do not enable recording on CPU.
- No public exception translation policy changes and no fallback to fresh graph
  construction on recording failure. Existing typed results remain authoritative.

Useful eager Scene setup count intentionally changes Part25B0 to1; subsequent
scene sample construction becomes0. Quiescent resetDiagnostics does not destroy
sessions: existing scene/CPU sampling in a new epoch creates0 new objects.
Successful model scan creates1 session and N samples; repeated prepare creates0.
Unsupported/timeline-rejected preparations create0. Sampled late failure creates
one session per retry and counts actual attempted frames, never publishes partial
model. The confirmed overflow fixture fails on frame1 after frame0; each attempt
must count2 samples, and recovery/typed error must match ordinary evaluation.

## Verification

Source foundation tests first reproduce stale ID snapshots and eviction using
the current behavior, before implementation. Test all four path/four paint
owners with live geometry, cold-source exact sample before preparation, later
prepare on same Instance, preserved old copies, hidden/visible and multi-path
cases. A dedicated process uses private LottieLoader::configureModelCacheSize(1)
for deterministic eviction before preparation and after preparation/new-instance
creation. The existing namespaced public cache setter has an unrelated linker
defect; do not expand scope to fix it.

Model parity uses an independent oracle: same JSON/hash/descriptor/cache key,
explicit parsed extraction, a newly constructed ordinary Animation per ascending
frame, unchanged bridge/updateAssetModel/finalizeAssetModel, then model-applied
scenes compared through unchanged ExactSceneComparison. Assert each model's
assetHandle, source IDs, static canonical pointer address in its own model,
alias owner_before equivalence, and at least one live static geometry/paint.
Never use two changed Runtime scans as the model oracle.

Retain full access-order, viewport, epsilon, invalid-dimension recovery, CPU
isolation and two-Instance comparisons. Concurrent same-Asset preparation must
publish one shared pointer/attempt/session; two distinct Assets sharing source
must produce their own correct models while sessions construct/sample. No
same-Instance simultaneous access is required or newly supported.

Full MSVC gates: Telegram, Samsung with only its two baseline-proven Polystar
failures, explicit windows-msvc-win32-preview capture/WARP/device recreation,
none/Direct2D and installed external consumer. Run optimized Release lifecycle
and integration tests on both variants. Vendor patches must apply/reproduce
exact bytes; protected checkout fidelity and unchanged corpus/licenses checked.

## Measurements and limits

Preserve common schema2 timing instrument in apps/corpus_lab. Validate lifetime
counts with explicit fresh/persistent profiles, not weakened nonnegative checks.
Run sequential controlled A-B-B-A after builds/workers stop, 16 committed assets,
128square,1000 measured samples plus first frame,20 warmup,load-repeats1 and
cpu-repeats0. Record per-asset exact and full CPU-pipeline median/p95, setup,
first/steady counters and actual evaluator/projector storage bytes/generations.
Planner allocation telemetry remains unavailable, not inferred zero.

Measurement acceptance must validate the complete successful smoke-corpus
lifetime profile, not two counts derived from each other. Model sample count
equals that asset's manifest frame count. Setup metadata sessions=1 and CPU=0;
first/steady metadata/model/CPU sessions=0. Setup scene sessions are1/0/1 for
Original/Part25B/candidate respectively; first scene sessions1/1/0, steady scene
sessions1000/1000/0, and setup model sessions frames/frames/1. First/steady scene
sample counts are1/1000. The unchanged instrument exports only those scene
sample counts plus setup model samples; absence of model sampling in first/steady
phases must be covered by Runtime diagnostics tests, not invented TSV columns.
Validate schema,
successful statuses, alias correspondence and actual input hashes against the
committed corpus manifest before timing acceptance. Record and verify the actual
candidate build directory, Telegram variant, Release configuration and compiler
identity, not only a supplied executable hash. A structurally successful runner
exit is separate from the measured speedup/p95 target decision.

Primary original-runtime baseline uses the preserved c484f4b common-instrument
binary, SHA256614714992fbacd80c1714683080811b66f5192aa4fd8ed8cd70ab42cea514f3b
at out/benchmarks/part25b/baseline. Additional comparison against Part25B Runtime
uses out/benchmarks/part25c/baseline, SHA256be9b98b42fd0f610665163da3c676316bc14725a6cdf862c87ee2208f418da69.
These are different baselines and must be labeled separately. Both old binaries
remain untouched. Record current candidate hash/configuration and compiler.

Fix aggregation before seeing candidate timings, consistent with the Part25A
report: for each asset/statistic take the median of its two corresponding run
values (the average for two values). Across16 per-asset exact-median summaries,
take the median for each binary; Original aggregate divided by candidate
aggregate must be at least2. Each asset's candidate two-run p95 summary must be
at most1.10 times its Original summary. Also publish every A1/B1/B2/A2 median/p95
and per-asset ratio so aggregation cannot hide outliers. A median of reported
p95s is not a pooled-sample percentile or statistical confidence interval.
Zero denominators or invalid metrics mean the target is not established, never
an infinite speedup. Part25B ratios are descriptive, not the original target.

Observe current/peak process working set for1/16/64 live Instances on StickAndBall
and firework. Retained scenes/source leases may use more memory than Part25B;
report that tradeoff. Session counts do not prove zero allocation. The original
2x exact median/noassetp95>10% target is measured, not assumed. If unmet, report
partial performance acceptance honestly; do not claim Telegram-class production
performance on this smoke corpus. Private representative assets remain required.

## Delivery and approval

### Checkout fidelity addendum (2026-09-23)

Independent actual-checkout reproduction at
out/part25d-fixture-checkout/findings.md confirms another input family omitted
from Part25C byte protection. With the existing core.autocrlf=true setting,
26of29 tests/fixtures files change by exact LF-to-CRLF expansion; six smoke JSON
inputs no longer match the unchanged committed TGS payloads. Both the generator
integrity check and unchanged TGS Runtime source-identity test fail only in the
checkout. This is a repository transport defect, not an animation-algorithm defect.

Approve one bounded follow-up before persistent integration: protect
`/tests/fixtures/** -text`, consistent with the already protected corpus family.
Extend the existing disposable real-Git roundtrip regression with fixture JSON,
nested session JSON, and fixture TGS metadata/payload cases. First reproduce its
functional failure without the new attribute; then add the one attribute rule.
Hash every existing fixture before/after and preserve all bytes, licenses,
goldens, manifests and TGS payloads. Verify a newly exported corrected-index
checkout using core.autocrlf=true with the actual corpus generator and the
unchanged TGS Runtime test, not only the synthetic roundtrip.

Do not normalize runtime input/hash handling, regenerate corpus payloads, change
global Git settings or relax equality assertions. Those alternatives either
alter authored identity or leave clone portability dependent on local settings.
No dependency, API or renderer change is necessary. The existing SDD process
adds Task5 solely to keep completed/active task numbers stable; execution order
is Task2, Task5, Task3, Task4. Task5 has its own RED/GREEN/review boundary and is
serialized after the active Task2 writer. Controller approves this reversible
addendum under the user's delegated authority; possible cost is retaining
intentional mixed line endings in fixture documents, as required by byte fidelity.

Use SDD with one product implementer, independent review after each bounded
task, final whole-change review and fresh full gates. Split source foundation,
Runtime ownership, session integration/parity and measurement/report so each has
its own rejection boundary. Each vendor patch records source bytes/provenance
without changing dependency/archive/license identity. No unreviewed push.

Self-review: exact source ownership addresses both old/new Instance eviction;
epoch covers constructor and all builder writes including failure; no unchanged
frame mutex; CPU and shared final model remain isolated; tests use independent
oracles. The controller approves this written specification using explicitly
delegated authority. Implementation plan follows; Part25C must finish first.
