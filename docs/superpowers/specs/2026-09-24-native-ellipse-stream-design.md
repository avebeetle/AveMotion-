# Part26D — private native ellipse scene stream

Approved by the controller under the user's delegated reversible design decisions,
not a claim that the user reviewed this document. Part26C is accepted at6d154ca
(product6b0be0f), with exact remote equality and all final gates. This stage is
architectural; subagent-driven implementation starts only after the written plan,
self-review and preflight are recorded. Source references: the C certificate API,
out/part26d-design/emitter-contract-audit.md and canonical-promotion-note.md.

## Intent and alternatives

User wants an own Lottie/TGS engine statically embedded in the future application's
Avelabs-UI EXE, not a separate AveMotion DLL. The existing Motion Lab is reference
CPU and must remain honestly labelled. D is one independently verifiable step:
compute supported scenes without asking rlottie to evaluate each frame.

1. Recommended: private reference-assisted preparation, independent native scene
   stream, full scene/history/plan differential gates. Small observable contract;
   C remains an oracle bridge, not the final native compiler.
2. Activate a production Runtime route now: reduces temporary API surface but
   changes fallback/public behaviour before geometry and provenance proof. Reject
   for D; no public route selects the stream.
3. Rewrite loader, player and raster integration together: reaches the desired
   product surface sooner in code shape but confounds parsing, geometry, timing
   and ownership defects. Split into subsequent stages with working gates.

D does not supply independent loading, full playback, pixels, generic ellipse or
all-Lottie support, speedup, zero allocations or upstream ID-origin provenance.

## Global Constraints

- Work directly in main; scoped commits and ordinary push only, no history rewriting.
- Preserve Part25I admission grammar, codes, paths and resource limits.
- Keep D private and Telegram-only; none/Samsung must not acquire D sources or tests.
- No ordinary production caller selects native emission; do not change Runtime.cpp, public APIs/fallback, Player threading, Direct2D ownership or ANGLE/backend coverage.
- Do not modify vendor sources, versions, patches, licenses/notices, committed fixtures or goldens.
- Do not add dependencies, change Windows settings, write/build/run GUI in Avelabs-UI, or recreate UI/out.
- Preserve raw evidence and SDD records; do not create or resume an automation.
- Exact comparisons use no epsilon, fingerprint-only shortcut, cached reference geometry or weakened expected output.
- One product implementer at a time; TDD and independent task/whole-stage review before ordinary push.

## Private stream contract

Place NativeEllipseStream.hpp/.cpp in src/render and compile into Rendering only
under the Telegram reference guard. Rendering already links Runtime and owns the
primitive generator. Do not add Rendering as a Runtime dependency or compile the
generator implementation twice. No public installation/export of the private API.

NativeEllipseStream is noncopyable, serial, created from a non-null immutable C
certificate plus explicit nonzero uint64 private instance identity. Checked
creation returns status and unique_ptr. Status names: Ready, InvalidCertificate,
InvalidIdentity, EvaluationPreparationFailed. Do not create a reference Instance
to obtain an identity. Scene InstanceHandle is invalid; scene.instanceId is the
private identity. This is not a registered Runtime instance.

emit(frame,width,height) returns private status plus owned EvaluatedScene only on
success. Status names: Emitted, InvalidViewport, EvaluationFailed,
UnsupportedNumericOutput, ModelApplicationFailed. Clamp integer frame to N-1;
zero viewport fails. Increment attempted sequence before validation; only success
updates prior fingerprints. No partial scene leaks on failure. Finite-check
derived transform/paths rather than treating finite authored numbers as proof.

Own only const certificate/model, prepared PropertyEvaluator, private workspace,
sequence and previous fingerprints. No retained Runtime, ordinary Instance,
Animation/tree, oracle callback, per-frame oracle geometry or cross-stream mutable
cache. Returned vectors own their bytes; workspace views never escape.

### Named interface

Namespace avemotion::render::detail. Private header NativeEllipseStream.hpp
declares NativeEllipseCreateCode and NativeEllipseFrameCode with the status names
above; NativeEllipseCreateResult owns unique_ptr<NativeEllipseStream> stream and
code/message, and is truthy only for Ready plus non-null stream.
NativeEllipseFrameResult owns optional<runtime::EvaluatedScene> scene and
code/message, truthy only for Emitted plus a present scene.

NativeEllipseStream::create(shared_ptr<const runtime::detail::NativeEllipseCertificate>,
uint64_t instanceId) returns NativeEllipseCreateResult.
NativeEllipseStream::emit(size_t frame,size_t width,size_t height) returns
NativeEllipseFrameResult. Constructor is private; delete copy operations.
The caller supplies distinct nonzero identities for simultaneously used streams;
no registry, global allocator or new Runtime handle API belongs in this proof.
Private fields may directly own the evaluator/workspace; no PImpl is required.
Use a noncopyable class with a declared out-of-line destructor if needed by types.

## Observable output

Target ordinary model-applied evaluateModelFrame semantics, not projected scene
semantics. Reuse PropertyEvaluator, generateEllipsePath, applyAssetModel and
computeSceneFingerprints. Resolve bound node/property IDs, never fixed indexes.

Reproduce C's root/shape layer IDs, keyPaths, parent/child/draw ranges and activity
visibility. Bind active draw's typed render/source IDs, solid/local byte paint,
fill/stroke/default inactive paint fields, opacity and local flags. No extra
draws, masks/clips/repeaters. Generate local and final paths independently from
position/size/direction and world+viewport transform. Distinguish raw geometry from
canonical promotion: Runtime::evaluateFrame may clear a static localPath after
attaching canonicalGeometry; that is not evidence of viewport culling. D targets
model-applied scenes and must follow applyAssetModel's exact ownership behaviour.
Empty generated paths retain bridge-compatible hash0. Nonempty hash
uses verb count, point count, verb bytes and float x/y bits. Bounds/statistics use
the same counted final geometry as the ordinary scene.

Keep sourceGeometryProjected false, revision0/non-authoritative and other ordinary
compatibility defaults. applyAssetModel attaches the exact frozen model and
canonical aliases with its control block, typed resource IDs and resource origins.
Do not substitute equal standalone canonical objects.

Scene comparator excludes identity, sequence, changes and upstreamChangeBits.
Check first three explicitly; document upstreamChangeBits as excluded because it
is an upstream implementation-history detail, native output SceneChangeNone. Do
not advertise literal equality of every field while that exclusion exists.

History: first successful result has default all-true changes; later successful
results compare the five fingerprint/history fields as Runtime does. Failed
requests do not become history; repeated same frame/viewport has no visual change.

## Independent expected output

Test helper owns exact source text, distinct ordinary source/cache identity and
its independently built frozen model. Explicit parsed extraction stamps each
ordinary source before fresh Animation sampling. The candidate and oracle must
not share mutable LOTModel/Animation/workspace/scene vectors/planner history. Keep
an ordinary source lease for its whole use and verify independence using existing
private animation-access test mechanisms. Do not toggle global caches concurrently.

For every expected request build a fresh ordinary Animation and extract its tree,
then apply the independently frozen model. Before planner comparison normalize only expected scene assetHandle to the
candidate's asset handle, expected instanceHandle to invalid, instanceId to the
same supplied private identity, and evaluationSequence to request ordinal. This
normalizes comparison keys, not real ownership. Keep the independently frozen
expected model and separately inspect real candidate/expected model alias owners.
Use request ordinal, not frame+1, for sequence. Independently compute changes, and
cross-check sustained ordinary Instance changes on the same request sequence.

Add a test-only full RenderPlan comparator: stamp fields, every draw/cache key,
geometry and paint updates, all transforms/opacities/bounds/features, plan bounds,
dirty region, statistics, fingerprints and history booleans; exact sourceScene
comparison plus separate owner relationship checks. Functional mutation tests
must show that wrong stamp/update/history/bounds fields are detected. Existing
fingerprint or HeadlessPlanBackend-only comparisons are insufficient.

## Required differential matrix

Use existing committed fixture plus in-memory, named transformations (no fixture
or golden changes): animated baseline, static visible position, static far outside
[-32768,32768], active [10,20), fractional rate/coordinates/translation, nonsquare
logical size, solid byte-boundary colors, linear and nonlinear admitted easing,
equivalent decimals/names, tiny positive surviving/collapsing size. Keep the C
certificate result separate from D success. Include viewport-edge-crossing motion
and near-boundary translation without presupposing upstream culling semantics.

Evaluate every integer frame at 512x512,256x256,384x256,256x384, forward/reverse and
repeated endpoints/activity boundaries, repeated interior and alternating viewport.
Print planned and executed counts by fixture/order/viewport. Log first mismatch
field/frame/viewport/ordinal and float bits. No epsilon, weakened goldens, selected
post-failure samples or cached reference geometry. A C-eligible input that fails D
is a documented emitter limitation, not grounds to silently narrow raw admission.

Plan/history cases: disappearance/reappearance, repeat/reverse, viewport-only,
presentation translation/opacity/visibility/layoutRevision, stale-sequence refusal.
Independent native/ordinary planners receive corresponding request order. Two
native streams have separate workspaces/history/animated geometry cache identities,
but static paint and static-position geometry asset resources share correctly.

Lifetime: emit after factory Runtime/Instance destruction and after external
certificate/Asset owners are gone; retained scenes/plans and canonical aliases
remain valid after streams die; weak owners expire only after final aliases/plans
are released. Bounded concurrent two-stream runs are not TSan claims.

CPU isolation: ordinary CPU frames before/after native work equal independent
ordinary CPU output. This proves no interference, not an own CPU renderer.

## Reference-work proof and gates

Keep supplied Runtime used for C creation alive and quiescent. Capture all actual
metadata/model/scene/CPU session and sample counters immediately before native-only
creation/emission and after; all deltas zero. C's N model plus N scene samples are
reported cold preparation cost. Oracle calls are outside the interval and explicitly
counted by the harness. Also inspect emitter dependency path: no reference includes,
Runtime/Instance construction, Asset::prepareModel or oracle callback.

Task1: emitter, independent expected scene helper and scene differential matrix.
Functional RED is successful build but absent native baseline scene. Task2: complete
plan comparator with functional mutation RED, history/two-stream/lifetime/counter/
CPU isolation. Independent review after each; root fresh full Telegram/Win32
preview(capture/WARP/device recreation)/none, vendor/corpus/private-source boundary
gates and whole-stage independent review. No benchmark speed claim in D.

## Following bounded stages

Own bounded JSON reader with exact decimal/rejection compatibility; own supported
model/ID/resource/paint construction and none-build JSON/TGS API; reference-neutral
Player timeline registration; native plans through owned Direct2D/WARP raster and
readback; isolated Avelabs worker-owned QImage output and acceptance. Do not use
reference placeholders in Player to claim reference-free playback. Protect accepted
main UI EXE and use build/cmake or build/diagnostics, never recreate UI/out.


## Controller self-review and scope approval

No placeholder or contradictory route activation found. Private Rendering placement
avoids the Runtime/Rendering cycle; existing public dependency on Evaluation is
transitive through Runtime. Model-application versus canonical-promotion semantics
are distinguished. C does not guarantee geometry parity; failing exact comparisons
must be diagnosed, not hidden. This approval authorizes the D written plan only;
subsequent loader/timeline/pixel/host designs remain separate.
