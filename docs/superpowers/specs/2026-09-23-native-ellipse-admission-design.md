# Part 25I — strict input admission for the one-ellipse native slice

Status: controller-approved under the user's delegated reversible design/spec/
plan authority, 2026-09-23. Architectural prerequisite only; no native playback
route is enabled by this stage.

## Intent and evidence

AveMotion is becoming a lean Windows animation module for AveVoice. Part25D
removed repeated reference session construction, but frame production still
samples the reference. Part25H's throwaway one-slot candidate independently
generated dynamic geometry and matched 1,008 pre-model ordinary-reference scenes
for two fixed fixtures, including inactive ranges and four viewports. Controller
reruns passed. It learned immutable render identities and static paint once; it
did not prove model application, planner/history behavior, arbitrary inputs or
performance. Evidence: out/part25h-native-proof/findings.md.

Part25G proved unknown and merge operators can vanish from the parsed model.
An input allowlist is therefore a prerequisite, not a renderer-ready certificate.
This stage implements only that gate and its adversarial tests. No filename,
fixture hash or observed parsed graph may substitute for inspecting raw input.

## Alternatives and selected boundary

1. Selected: private Runtime input audit, using the already pinned Telegram
   RapidJSON headers. It examines raw JSON completely and returns an explicit
   accepted/rejected result. No new dependency, exported target or public API.
2. Upstream parser omission reporting would also solve dropped-input detection,
   but touches vendor machinery and provenance beyond this bounded prerequisite.
3. Trusting parsed node kinds/nativeDirect2DReady is rejected by the probe.

Compile the gate only for the Telegram Runtime variant. Its private header has
only standard C++ types. It is called by its test executable in this stage, not
by Asset load/preparation, evaluation, validator or fallback selection. The static
archive's unreferenced object does not add a playback path. Samsung and offline
Runtime do not acquire RapidJSON dependencies. Primitive ownership is unchanged
until an emitter actually needs that move.

## Exact admitted grammar

Every object has an exhaustive allowed-key set; all listed keys are required
unless marked optional. Keys may appear in any order. Duplicate keys anywhere,
unknown keys anywhere and extra children are rejected, even if upstream ignores
them. Optional `nm` is an inert string of at most 256 UTF-8 bytes without NUL.
Root optional `v` has the same string constraint. No other metadata is admitted.

| Object | Keys and values |
| --- | --- |
| Root | `fr`: finite number in (0,240]; `ip`: integer 0; `op`: integer 2..10000; `w`,`h`: integers 1..8192; `ddd`: integer 0; `assets`,`markers`: empty arrays; `layers`: exactly one layer; optional `v`,`nm` |
| Layer | `ddd`:0; `ind`: positive integer <=2147483647; `ty`:4; `sr`:1; `ks`: layer transform; `ao`:0; `shapes`: exactly one group; `ip`,`op`: integer active range 0<=ip<op<=root.op; `st`:0; `bm`:0; optional `nm` |
| Layer transform | `o`: static scalar100; `r`: static scalar0; `p`: static vec3 with finite x/y in [-32768,32768], z0; `a`: static [0,0,0]; `s`: static [100,100,100] |
| Group | `ty`:"gr"; `it`: exactly ellipse, fill, transform, in that order; optional `nm` |
| Ellipse | `ty`:"el"; `d`:integer1; `s`: static vec2, each component >0 and <=16384; `p`: static vec2 or animated position defined below; optional `nm` |
| Fill | `ty`:"fl"; `c`: static vec4 with RGB in [0,1] and alpha1; `o`:static scalar100; `r`:integer1; optional `nm` |
| Group transform | `ty`:"tr"; `p`,`a`:static [0,0]; `s`:static [100,100]; `r`,`sk`,`sa`:static scalar0; `o`:static scalar100; optional `nm` |
| Static property | exactly `a`:integer0 and `k`:scalar/vector specified by owner; vector length exact, every element numeric and finite |
| Animated position | exactly `a`:integer1 and `k`:two keyframe objects; position components finite in [-32768,32768] |
| First position keyframe | exactly `t`:integer0, `s`,`e`:vec2, `i`,`o`:easing objects |
| Final position keyframe | exactly `t`:integer(root.op-1), `s`:vec2 numerically equal to first.e |
| Easing object | exactly `x`,`y`:scalar finite numbers in [0,1]; no arrays, hold, spatial tangents or expressions |

Numeric values are mathematical comparisons after finite-number validation;
integer requirements accept JSON numbers with an exactly integral value within
the stated bounds. Booleans/strings/null never count as numbers. Negative zero
is numerically zero here; this gate makes no float-bit rendering guarantee.
Root names, layer names, colors, dimensions and positions are not fixture-pinned.
Any missing required key, invalid type or value rejects the native slice.

## Parser and resource contract

- Maximum input: 1,048,576 bytes; empty input rejects. Embedded literal NUL bytes
  reject before parse, including a valid JSON prefix followed by NUL/trailing data.
- Parse the complete bounded buffer with UTF-8 validation and iterative parsing
  using existing RapidJSON flags. No stop-when-done, comments, trailing commas,
  NaN/Infinity allowance or permissive number conversion. Trailing whitespace is
  accepted; trailing non-whitespace and invalid UTF-8 reject.
- Before semantic recursion, walk the DOM iteratively and reject more than 4096
  values, container nesting deeper than 32, or duplicate member names. Count the
  root as one value and depth1; each object member value/array element adds a
  value, and containers increase nesting depth. Keys are not values.
- Accepted grammar is shallow and bounded. No reference/runtime/backend calls,
  source-model mutation, static state or retained string_view/DOM pointers.
- A reject is only native-input ineligibility, never a new public load error.

## Private result and diagnostics

`src/runtime/NativeEllipseAdmission.hpp` defines, in
`avemotion::runtime::detail`:

```cpp
enum class NativeEllipseAdmissionCode {
    Accepted, InvalidJson, ResourceLimit, UnsupportedField,
    InvalidType, UnsupportedValue, UnsupportedStructure
};
struct NativeEllipseAdmission final {
    NativeEllipseAdmissionCode code;
    std::string path;
    [[nodiscard]] bool accepted() const noexcept {
        return code == NativeEllipseAdmissionCode::Accepted;
    }
};
[[nodiscard]] NativeEllipseAdmission auditNativeEllipseInput(std::string_view json);
```

Accepted has an empty path. Rejected has a nonempty JSON-pointer-style location
beginning with `/`; use `/` for document-wide failures and escape `~`/`/` in keys.
Codes distinguish syntax/duplicate/encoding failures (InvalidJson), resource
limits, unknown fields, wrong value type, unsupported values and wrong shape/
array/missing-field structure. Deterministic first failure is sufficient; this
is not a public diagnostics stability promise. Allocation failure may propagate
normally; the API is not noexcept and does not claim a zero-allocation guarantee.

## Acceptance gates and non-goals

Functional RED must exercise real JSON through a compilable reject-all initial
seam before implementing semantics. Then test the baseline, meaningful accepted
variants and rejection mutations at every grammar level, duplicates, malformed
encoding, numeric bounds and parser resource limits. Include literal-NUL suffix,
unknown/mm/hidden operators, known-object unknown fields, expressions/tangents,
reordered valid keys and escaped inert names. Every rejection returns a path.
The existing 16-asset smoke corpus and all goldens remain byte-identical.

Fresh Windows Telegram configure/build/full CTest and explicit Win32 preview
configure/build/full CTest must pass. Fresh none build/full CTest proves no new
offline dependency. All-vendor/corpus integrity and independent review precede
ordinary push. Record actual Linux CI separately; existing Linux exact golden
failures are not waived or reclassified by this stage. Samsung stays optional
comparison, with notices/source/presets unchanged.

No public API, playback routing, model publication, CPU path, Direct2D ownership,
Player threading, ANGLE/backend coverage, fallback policy, vendor/golden change,
dependency installation, Windows setting or licensing change. No new benchmark.

After this gate, a separately reviewed spec must join raw admission to authored
model correspondence and scan-time render identity verification, then prove
model-applied scene/plan/history parity, old-result ownership, two-instance/CPU
isolation and zero production reference samples before enabling native playback.
