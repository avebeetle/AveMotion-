# Part26B — owned native ellipse input

Date: 2026-09-24. Status: controller-approved under the user's explicit
delegation of reversible design/plan decisions. This does not claim that the
user read this new document. Interactive continuation; the completed Part26A
heartbeat remains paused.

## Intent and boundary

The end goal is an AveMotion-owned Lottie/TGS engine statically embedded in
AveVoice/Avelabs, with rlottie available as a comparison oracle, not required
in the final product. Part26A supplies a reference-CPU test host, not that engine.
Part25I supplies a strict one-ellipse admission gate, but discards validated
values. Part26B turns its validated input into an owned typed description.

Success means exact, independently retained data for every currently admitted
input, without changing a single admission classification or playback route.
This is an input-side prerequisite, NOT model certification, native playback,
reference-free packaging, SVG support, GPU acceleration or broad Lottie support.

## Alternatives and decision

1. **Selected: owned exact descriptor first.** Extend the existing validation
   pipeline and retain typed values. Small, directly testable ownership and
   numeric contract; model/slot correspondence remains a subsequent gate.
2. Combine descriptor and frozen-model correspondence now. Useful sooner for
   eligibility, but couples raw decimal semantics to float model conversion,
   source-node topology and scan-time render slots in one review surface.
3. Replace the entire ingestion/compiler immediately. Too broad: the current
   private parser dependency and source-to-render correspondence need explicit
   independent designs, not an unreviewed wholesale rewrite.

The read-only audits in `out/part26b-native-design/` support option 1. In
particular, Part25I accepts positive `1e-9999` for rate/size. A float descriptor
would either silently lose it or narrow the existing grammar.

## Global Constraints

- Work directly in main; ordinary scoped commits/push only, no history rewriting.
- Keep Part25I admission codes, paths, accepted grammar and resource limits unchanged.
- Preserve exact decimal values; no floating-point conversion or new precision/exponent cap.
- Keep the descriptor private and Telegram-only; the none/Samsung build must not acquire this parser.
- Do not change public Runtime APIs/fallback, Player threading, Direct2D ownership or ANGLE/backend coverage.
- Do not modify vendor sources, versions, patches, licenses/notices, fixtures or goldens.
- Do not install dependencies, change Windows settings, or write/build/run GUI in Avelabs-UI.
- Preserve raw evidence and SDD records; do not resume or create an automation.

## Data contract

New private header `src/runtime/NativeEllipseInput.hpp`, namespace
`avemotion::runtime::detail`; standard C++ only, no RapidJSON/rlottie types.

`NativeEllipseDecimalPower { bool negative; std::string magnitude; }` and
`NativeEllipseDecimal { bool negative; std::string digits;
NativeEllipseDecimalPower power; }` own the canonical mathematical value
`sign * integer(digits) * 10^(signed power)`. Defaults represent zero. Zero is
exactly positive `digits="0"`, positive power `magnitude="0"`. Nonzero digits
have neither leading nor trailing zeros; exponent magnitude has no leading
zeros and exponent zero is positive. Negative zero normalizes to zero.
Copy the existing validated exact-number metadata, do not reimplement decimal
normalization or expand powers into enormous strings. No conversion to float,
double, `strtod` or DOM `GetDouble` is allowed in this path.

`NativeEllipseVec2` is `std::array<NativeEllipseDecimal, 2>`.
`NativeEllipseStaticPosition { NativeEllipseVec2 value; }` and
`NativeEllipseAnimatedPosition { uint32_t firstFrame, lastFrame;
NativeEllipseVec2 start, end, incoming, outgoing; }` are alternatives of
`NativeEllipsePosition` (`std::variant`). Easing pairs store `[x,y]` directly
from first-keyframe `i` and `o`; the last keyframe's start equals `end` by the
existing exact admission rule. No spline evaluation or interpolation occurs.

`NativeEllipseInput` owns these fields:

- `uint32_t width, height, endFrame`; `NativeEllipseDecimal frameRate`;
- `int32_t layerId`; `uint32_t layerInFrame, layerOutFrame`;
- `NativeEllipseVec2 layerTranslation, size`; `NativeEllipsePosition position`;
- `std::array<NativeEllipseDecimal, 4> fillColor`;
- `std::optional<std::string>` fields `version`, `name`, `layerName`,
  `groupName`, `ellipseName`, `fillName`, `transformName`.

All value structs support defaulted equality for direct field-wise tests.
Optional names preserve absence versus empty string and decoded UTF-8 contents.
Structural integers come from exact validated integer extraction, never a
rounded DOM conversion. Fixed profile semantics are implicit: root ip=0,
one shape layer/group, ellipse direction=1, fill rule=1, alpha=1, opacity=100,
identity transforms except layer XY translation, no hidden/mask/matte/operators.
The exhaustive grammar and limits remain the Part25I design/code; this carrier
cannot represent a more general shape program and must not be presented as one.

`NativeEllipseInputResult` contains `NativeEllipseAdmission admission` and
`std::shared_ptr<const NativeEllipseInput> input`; its explicit bool conversion
requires both accepted admission and nonnull input. The entry point is:

```cpp
[[nodiscard]] NativeEllipseInputResult decodeNativeEllipseInput(std::string_view json);
```

Every accepted input returns a complete nonnull immutable object and empty
diagnostic path. Every rejection returns no object and the same code/path as
`auditNativeEllipseInput`. No partial publication, borrowed strings, DOM/tree
pointers, reference animation, mutable shared cache or process-global state.
Input buffer destruction/reuse cannot affect results. Independent concurrent
calls are safe; this does not claim a race-detector run. Allocation failure
may propagate normally, matching existing admission behavior.

## Implementation boundary

Keep the existing `.cpp` and private exact arithmetic. Share the existing JSON
parse/resource/numeric-metadata/audit pipeline between both entry points.
After successful audit, materialize the descriptor from the SAME validated DOM
and exact metadata before destruction. No second semantic grammar/parser.
Audit-only calls should retain their current no-descriptor path via an optional
output pointer/internal flag, so this feature adds no unused allocation there.
Use small field/vector/name copy helpers, not a general parser framework.

No production caller is added. The object remains in the existing Telegram-only
Runtime source list. Add one focused test executable in the same conditional
block as the admission test. TGS transport remains unchanged: a test decodes
the existing generated TGS fixture and compares its descriptor to JSON.
The admission 1 MiB limit remains applicable after TGS decompression.

## Verification

Functional RED: tests compile against a declaration/minimal stub but fail at
the assertion that valid input yields a descriptor. Then implement extraction.
Assert complete explicit fixture values, not merely `accepted=true`.
Cover animated/static variants, active interval [10,20), layer IDs/translation,
size/color bounds, names/order/absence/empty/UTF-8, exact equivalent spellings,
underflow and long exponent values, near-boundary rejection, duplicate/unknown
fields and resource failures. Rejected calls must not publish after a prior
success. Retain results after source mutation/destruction; concurrent calls
with distinct contents must not alias writable state or cross-contaminate.

Run unchanged admission tests alongside the descriptor tests. Fresh full MSVC
Telegram, explicit Windows preview and no-reference CTest gates, vendor verify,
TGS generator --check, and generated no-reference compile graph inspection.
No performance or zero-allocation claim is needed for this cold private step.
Record exact commands/results and independent task/final reviews in a report.

## Follow-on sequence, not implemented here

1. Raw descriptor -> frozen model correspondence and scan-slot ownership,
   source identity, explicit exact-to-model conversion eligibility contract.
2. Native scene emission -> full-field model-applied scene/plan/history parity,
   retained results, two instances, CPU isolation and no per-frame reference calls.
3. Separately designed independent parser/compiler boundary and no-reference
   successful loading; license/dependency decisions cannot be inferred here.
4. Broader supported grammar and owned corpus, real workload measurements,
   then host rendering/presentation optimizations based on evidence.

Self-review: each variable profile field is owned; constants are documented;
exact-number and existing acceptance promises agree; no new external authority,
route activation, dependency redistribution or UI change is implied.
