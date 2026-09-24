# Part26C — Native ellipse model and render-slot correspondence

Status: controller-approved under the user's delegated reversible technical
decisions on 2026-09-24. This does not claim user review of this document.
Base: d212a49. Evidence: out/part26c-design/{topology-audit,scan-audit}.md and
model-baseline/parsed_model_details.tsv. This is a proof boundary, not playback.

## Goal and continuation

Part26B owns exact admitted input. Part26C proves that this input corresponds
to an immutable authored model and exactly one observable render slot across
the complete source timeline. Next: Part26D native scene emission and full
scene/plan/history differential gates; then separately designed independent
ingestion/model construction in the none build; then own rendering in Motion
Lab. Completion of C is not completion of the user's long-work request.

The first supported own path remains the admitted one-layer/group ellipse and
solid-fill subset, not full Lottie/SVG support. Reference CPU lab, a certificate,
native geometry projection and a reference-free player are distinct milestones.

## Constraints

- Work directly in main; ordinary scoped commits/push only, no history rewriting.
- Preserve Part25I admission codes, paths, grammar and resource limits.
- Keep new code private and Telegram-only; none/Samsung must not acquire it.
- No production caller selects the new factory or native route in this stage.
- Do not change Runtime.cpp, public APIs/fallback, Player threading, Direct2D ownership or ANGLE/backend coverage.
- Do not modify vendor sources, versions, patches, licenses/notices, fixtures or goldens.
- Do not install dependencies, change Windows settings, or write/build/run GUI in Avelabs-UI.
- Preserve raw evidence and SDD records; do not resume or create an automation.

## Selected approach and alternatives

Use a private cold factory invoked only by tests. It owns the exact input bytes,
decodes them, loads those same bytes with a fresh public Runtime, prepares its
model, binds the descriptor, and scans a separate Instance at every integer
frame and logical viewport. Ordinary model preparation already scans N frames;
this certification scans N additional scene frames. Count both, and make no
speedup claim. No ordinary caller pays for this proof.

An observer inside prepareStableAssetModel could avoid the second scan, but
would introduce production publication/cache coupling before correctness is
established. Defer that integration. A certificate from the union model alone
is insufficient: it loses duplicate draws, visibility and per-frame bindings.

## Numeric interpretation (distinct from raw admission)

Use the canonical decimal digits/sign/exponent already retained by Part26B.
Convert a compact scientific token with locale-independent std::from_chars
into double, checking full consumption, range and finiteness; never expand
arbitrary powers into zeros or use pow. For model float fields, check float
range before casting, then reject a nonzero decimal becoming zero. The pinned
parser reads double and stores float, so double-then-float is deliberate.
Ordinary rounding is allowed; exact zero becomes positive zero. Equality to
the model uses numeric equality, no epsilon. Structural integer fields widen
exactly. Frame rate is ALSO stored as float by pinned LOTCompositionData,
then exposed as double: compare double(convertedFloatRate), not original double.

All varying vector/color/easing/translation/size components follow this rule.
Nonzero subnormals are eligible if they survive conversion and match the model.
Overflow, underflow to zero, malformed private descriptors or nonfinite model
values are ineligible. Accepted raw 1e-9999 remains accepted by admission but
cannot receive a binding. Distinct decimals rounding to the same float are not
silently changed in the owned descriptor; this certificate proves the stated
pinned numeric interpretation, not lossless binary representation.

## Task1: authored binding contract

Private NativeEllipseBinding.hpp/.cpp, namespace avemotion::runtime::detail:

```cpp
enum class NativeEllipseBindingCode {
    Bound, UnsupportedNumericConversion, InvalidModelTable,
    CompositionMismatch, TopologyMismatch, SourceIdentityMismatch,
    PropertyShapeMismatch, ValueMismatch, TrackMismatch
};
struct NativeEllipseModelBinding {
    model::SourceNodeId root, layer, group, ellipse, fill;
    model::PropertyId layerTransform, layerOpacity, groupTransform, groupOpacity;
    model::PropertyId position, size, color, fillOpacity;
};
struct NativeEllipseBindingResult {
    NativeEllipseBindingCode code = NativeEllipseBindingCode::InvalidModelTable;
    std::optional<NativeEllipseModelBinding> binding;
    explicit operator bool() const noexcept; // Bound AND present
};
NativeEllipseBindingResult bindNativeEllipseModel(
    const NativeEllipseInput&, const model::MotionAssetModel&);
```

The binder is pure, does not mutate/cache/retain the model or evaluate a frame.
It validates schema/direct parsed model, metadata and the complete authored
subset; factory source ownership/hash/handle checks are separate Task2 work.
Every failed result has no binding; categories identify the failing boundary.

Resolve one composition's root, then checked child edges root -> shape layer
-> shape group -> [ellipse, fill] in authored order. Exactly five present
source nodes, eight owned properties, no hidden/extra/dangling records or
edges. IDs are table addresses, not hard-coded semantic positions. Check ID
and present flags, index ranges without arithmetic overflow, parent and
composition backreferences, invalid transform parent/referenced composition,
exact ownership and unique property semantics/index0. Correctly renumbering
nodes/properties and updating all references must still bind.

Root composition is named root, logical dimensions/timeline [0,endFrame),
converted frame rate. Layer has Shape kind, admitted authored ID and activity
[layerInFrame,layerOutFrame), start0/stretch1, no authored parent. All nodes
have supported defaults: enabled, no masks/matte/blend/gradient/stroke/trim/
repeater/asset reference/auto-orient/hidden semantics; ellipse clockwise, fill
winding. Match authoredStatic/dependency semantics to static versus animated
position (root/layer/group inherit animation; fill always static).

Match effective source names and hashName (core FNV appendString): missing or
empty layer -> layer:<id>; group -> group; ellipse/fill -> source-node. Root
name is root. Descriptor root name/version/transform name are retained but
not represented in these model rows; do not pretend to match absent metadata.

Required properties: layer translation-only TransformMatrix plus opacity100;
group identity TransformMatrix plus opacity100; ellipse size Vec2; ellipse
position Vec2; solid fill Color with alpha1 and opacity100. Static properties
have only Static flag, valid correctly typed value, no track. No unused extra
authored property/track/segment or unsupported value tables. Validate accessed
value indexes/types and finite exact interpreted values, not their array order.

Animated position has only Animated flag, no static value, one track and one
segment, owner/property/track backreferences, [firstFrame,lastFrame), exact
interpreted start/end Vec2 and outgoing/incoming temporal controls. No spatial
interpolation/tangents. Linear only for controls (0,0),(1,1), otherwise CubicBezier.
The raw terminal keyframe is not a second model segment. Layer activity does
not truncate the track. No flags/dependencies may introduce an extra operation.

## Task2: observable full-timeline certificate

Add NativeEllipseCertificate.hpp/.cpp and NativeEllipseScanAudit.cpp. The
header defines private value-owned slot metadata and results; the factory and
scan validation are separate files. No installed header or public friend.

Expose `prepareNativeEllipseCertificate(std::string_view)` and the private
overload `prepareNativeEllipseCertificate(Runtime&, std::string_view)`.
The one-argument form constructs a local Runtime and delegates. The overload
still owns/loads the exact bytes itself, never accepts an arbitrary Asset, and
does not retain the Runtime. It permits a test to keep that same Runtime alive
for real diagnostic deltas during later native emission. Return
`diagnosticsBefore` and `diagnosticsAfter` snapshots, including on failure;
caller keeps the supplied Runtime quiescent while measuring deltas. No public
API or production Runtime.cpp change. Also expose a scan audit
class for direct mutated-scene tests. The audit takes const admitted input,
Task1 binding, frozen model and expected AssetHandle/source hash, then
`observe(std::size_t frame, const EvaluatedScene&)` in strict 0..N-1 order and
`finish()`. An invalid observation poisons that audit; finish cannot publish
early, after skipped/repeated frames or after failure. Scan error categories:
identity, timeline, layer layout, draw multiplicity, source binding, resource
binding, unsupported slot, incomplete scan. Keep failure results empty.

Certificate bundle owns exact JSON string, const descriptor, const Asset lease,
const frozen model and the verified value metadata. Factory result reports
admission, binding/scan category or reference preparation error separately and
the two diagnostic snapshots (also on failure). It publishes const shared ownership only
on success. No scene, Animation, Instance, render-tree or mutable upstream
pointer is retained. Asset cannot own the certificate, avoiding self cycles.
Reject over-limit input before copying its bytes or loading the reference;
the existing admission result remains the authority for code/path. Bounded
accepted bytes must be owned before reference load and retained in the bundle.
Bind consumers to the exact asset object, handle including generation, hash and
frozen model pointer; hash alone is not proof of identity. A different load of
the same bytes cannot consume this certificate. Never expose a constructor
that lets callers pair an unrelated asset with admitted bytes as certified.

For each frame, check scene identity/hash/frame/viewport; exactly root and shape
layers, stable IDs/parentage/keyPaths/child references, no masks/clips/matte,
opacity1; root visible, shape visible exactly inside the activity interval.
Inside require exactly one draw, outside none (including layer draw ranges).
Remember only fixed metadata, not paths/transforms/bounds/fingerprints:
two layer IDs and keyPaths, child layout; unique typed draw/node/geometry/paint
IDs, layer index/order; source path/paint IDs and path count/modifier flag;
local availability/static flags, fill rule, solid/local solid RGBA bytes,
separated opacity flags/value and declared model resource counts. Stroke is
disabled/default, no gradient/image/repeater state. Reject active metadata drift.
Use focused structs, not a copied EvaluatedDrawItem with cleared vectors.

All active observations must bind ellipse/fill from Task1, sourcePathCount1,
modifier-free, local geometry/paint available, solid paint, expected static
flags. Final frozen model must have two coherent layer rows, exactly one
present node/draw, matching geometry/paint ownership and layer membership.
Paint is AssetStatic and matches observed local paint bytes; geometry is
AssetStatic for static position, InstanceEvaluated for animated position.
Check valid references/counts/ranges without assuming numeric IDs. No cached
per-frame geometry. A nonempty admitted interval guarantees an active sample.

Existing bridge substitutes scene indexes when upstream render IDs are invalid,
without exposing a provenance bit. C certifies OBSERVABLE binding consistency,
not absence of a coincidentally matching fallback ID. That stronger claim and
any production activation require a future private provenance seam or proof;
do not add a public API or silently assert it here.

## Verification and acceptance

Task1 functional RED must fail on a valid baseline binding, not compilation.
Tests: baseline/static/active-range/name/equivalent-decimal variants; fractional
frame rate preserving pinned float interpretation; semantic renumbering;
wrong owner/duplicate property/bad indexes/extra node or track/wrong names,
values, controls, times, flags, dependencies; numeric overflow/underflow with
unchanged raw admission and tiny supported nonzero conversion.

Task2 RED must fail because baseline certificate is absent. Tests cover all
frames for normal/static/[10,20) variants, boundary visibility, complete scan,
mutated duplicate/extra/missing draws, wrong source IDs/final rows/counts,
identity/handle mismatch, drift/unavailable local fields/static class demotion,
early finish/skipped/repeated frames and poison semantics. Retain certificate
after factory Runtime/Instance destruction; reject another same-byte asset.
Two factories may run independently; no shared mutable audit state. Assert N
model samples and N extra scene samples, zero CPU samples in successful factory,
using before/after deltas with a supplied Runtime, not unrelated live counters
or only an immutable end snapshot. Test both overloads and a nonempty Runtime.
Ineligibility must not change independent ordinary load/evaluation behavior.

Focused checks while iterating; one full Telegram suite per task before commit.
Independent review per task; final root fresh Telegram, windows-msvc-win32-preview
(capture/WARP/device recreation), none/Direct2D, vendor all and TGS16 integrity,
generated none/Samsung private-source boundary checks; whole-stage independent
review. Do not claim TSan, zero allocation, speedup, own ingestion or playback.
