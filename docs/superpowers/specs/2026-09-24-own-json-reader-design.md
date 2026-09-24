# Part26E — bounded first-party JSON reader

Controller-approved architectural design under the user's delegated reversible
decisions; not a claim that the user reviewed this document. Implementation waits
for D final acceptance and the written implementation plan/preflight. This is a
reader prerequisite, not the entire own Lottie loader or a public route change.

## Intent and measured reason for the split

The user wants an own animation engine embedded statically in the future Avelabs-UI
EXE, with eventual host acceptance there. Existing Motion Lab remains reference
CPU. D supplies privately tested own scene generation but still uses a Telegram
cold certificate. The next independent dependency to remove is JSON reading.

Two bounded probes preserve109 named observations in
out/part26e-characterization and out/part26e-numeric-characterization. Pinned parsing
accepts escaped lone low surrogate bytes but rejects the same raw UTF-8 bytes. Its
number conversion can reject one spelling and accept an equal-valued spelling:
1.7976931348623158e308 versus1.79769313486231580e308. A310-digit coefficient with
e-309 rejects despite exact value1; e-617 can parse as an exact nonzero small value.
These observations concern selected MSVC archives, not all inputs/platforms.

Alternatives:

1. Selected: private first-party lexical/value reader with explicit Unicode and
   exact-lexeme policy, built without rlottie; retain the existing admission APIs
   unchanged. Establish parsing, ownership, bounds and independent comparison
   before connecting a separately designed own admission/model compiler.
2. Transparent replacement of the old reader now: would require reproducing
   procedural float-conversion eligibility and malformed-surrogate behavior. A
   generic finite-double check is not equivalent; do not claim compatibility or
   silently alter current admission based on a finite matrix.
3. Repackage vendor parser headers: does not meet the first-party reader goal and
   introduces dependency/provenance decisions. Not selected; no vendor copying.

This chooses a staged, separately identified internal contract rather than a
hidden fallback or a new public compatibility promise. Exact-decimal normalization
remains unchanged and single-sourced in current admission. E does not duplicate it.

## Global constraints

- Direct main, scoped commits and ordinary push after TDD/review; no history rewrite.
- Do not change existing NativeEllipseAdmission/Input entry points, Part25I grammar,
  rejection codes/paths/limits, C binding/certificate, D emitter, Runtime.cpp, public
  fallback, Player, Direct2D ownership, ANGLE or the reference-linked install ban.
- No vendor/dependency/license/notice/fixture/golden changes; no copied or translated
  vendor implementation, new dependency or generated vendor constants.
- No Avelabs writes/builds/GUI or UI/out recreation; preserve its accepted EXE.
- No automation, Windows/settings/security changes or external publication.
- One product writer; independent task/whole-stage review. Preserve raw/SDD evidence.
- Do not label this reader a native animation loader, complete JSON-standard
  implementation, full Lottie support, reference-free playback, zero-allocation
  guarantee or measured speedup.

## Placement and scope

Add private src/formats/OwnJsonReader.hpp/.cpp to AveMotion::Formats in all variants.
The source includes only its own header and C++ standard headers. No dependency on
Runtime/Model/Evaluation/rlottie/Qt or another parser. Header remains outside public
include/install. Existing TGS decode stays unchanged; a test can pass its owned JSON
bytes to this reader. No ordinary production caller selects the reader in E.

Reader handles a complete JSON value, not just the supported ellipse keys. Unknown
subtrees must be syntactically checked before later admission. It retains insertion
order and duplicate keys until the resource pass; it does not coerce types or
convert numbers to binary floating point. No serializer, mutator, JSONPath, streaming
I/O, public options framework or general-purpose parser package is introduced.

## Named private result and ownership

Namespace avemotion::formats::detail. Entry readOwnJson(std::string_view) returns
OwnJsonReadResult with code, owned path and shared_ptr<const OwnJsonDocument> document.
Codes: Parsed, InvalidJson, ResourceLimit. Explicit bool is true only for Parsed
and non-null document. Success path is empty; root failure path is `/`. Failure
publishes no document. Allocation failure retains normal C++ bad_alloc behavior,
not an invented JSON classification.
Result also carries private OwnJsonReadStatistics: inputBytes, sourceBytes,
decodedBytes, nodeCount, nodeCapacity, framePeak, frameCapacity, nodeSizeBytes,
frameSizeBytes and resourceQueueCount, all size_t. Capture actual final/peak logical
counts/capacities before disposing failed scratch; no pointer or content leaks on
failure. These diagnostics are not an allocator/whole-process memory measurement.

Document owns its exact source bytes, a decoded-string byte arena and flat nodes.
Nodes use uint32 indexes, never internal pointers/string_views whose owner could
move. Root index0; invalid index UINT32_MAX. Kind has Null, Boolean, Number, String,
Object, Array. Each node records boolean value, firstChild/nextSibling, childCount,
key span for an object member, and data span for a string or raw number. Numeric
spans address owned source; strings/keys address decoded arena. Empty strings and
empty keys are distinct from absence according to parent/node kind, not strlen.

Expose read-only access to nodes/source/decoded spans, with checked node lookup and
span access. Do not expose mutable construction through the published const handle.
Convenience access need not mimic RapidJSON or add unused object-map machinery.
After source overwrite/destruction and document-handle copies, values remain exact.
Destroyed documents invalidate borrowed views as usual; no escaping workspace view.

## Syntax and scalar policy

Before parsing, bytes greater than1,048,576 return ResourceLimit `/`. Empty input or
any literal NUL byte returns InvalidJson `/`. Whitespace is exactly space/tab/CR/LF.
Require one complete value and only trailing whitespace. Reject BOM, comments,
trailing commas, NaN/Infinity, leading plus/zero, missing fraction/exponent digits,
bad escapes and unescaped control bytes. Iterative state machine, no recursive
descent or input-driven call-stack depth.

Validate raw UTF-8 as scalar values: reject overlong/truncated encodings, surrogate
encodings, out-of-range values and stray continuation bytes. Decode all JSON
escapes, including embedded zero, and valid high+low surrogate pairs. Reject lone
high/low surrogates and malformed pairs. This deliberately differs from the pinned
escaped-low-surrogate quirk; it is NOT a change to old admission in this stage.

Numbers follow lexical JSON sign/integer/fraction/exponent rules. Preserve every
raw byte including signed zero, exponent signs/leading zeroes and arbitrarily long
exponents within the input cap. Do not call strtod/from_chars for numeric eligibility,
expand powers, round, saturate the represented token or reject binary overflow/
underflow. Thus0e309 is a readable number here even though old admission calls its
document invalid. Mathematical admission and float materialization are later
separate contracts. Unknown branches use exactly the same lexical validation.

## Resources and first error

Complete syntax/encoding validation comes before the resource/duplicate pass,
except the initial byte/NUL gates. Parse into a compact arena bounded by input
bytes, not a prematurely enforced4096-node cap that could hide malformed suffixes.
Then breadth-first traverse from root, counting root as one value. Container depth
starts at1; increment only when entering an Object/Array child. A dequeued container
deeper than32 returns ResourceLimit at its owned JSON Pointer path. Traverse members
in source order and arrays in index order. Within each object compare decoded
length-aware keys against previous keys; a duplicate returns InvalidJson at that
key before enqueue/count checking. Before enqueueing a child when4096 values are
already present, return ResourceLimit at that child's path. This reproduces the
existing resource traversal and precedence for documents admitted by both scalar
policies, except the deliberate empty-ancestor diagnostic-path distinction below.

Pointer escaping is `~` -> `~0`, `/` -> `~1`; array indexes are decimal. Paths may
contain embedded zero from decoded keys and must own their bytes; callers must
not infer NUL-terminated text. No schema/name256-byte restriction at reader level.

Execution addendum (whole-stage review, controller-approved2026-09-24): preserve
every empty object-name segment in the own reader's pointer. The old helper treats
`/` both as root and as the first empty member, dropping a leading empty segment
when a descendant is appended. This is a third deliberate private-contract
difference, alongside numeric eligibility and scalar Unicode. For
`{"":{"a":1,"a":2}}`, own InvalidJson path is `//a`, legacy `/a`; for
`{"":{"":1,"":2}}`, own `//`, legacy `/`. A nonempty prefix
`{"outer":{"":{"a":1,"a":2}}}` produces `/outer//a` on both sides.
For a root empty member containing32 nested arrays, ResourceLimit paths are an
extra leading slash plus31 `/0` segments (own) versus31 `/0` segments (legacy).
For a root empty member containing4095 null children, ResourceLimit is `//4094`
(own) versus `/4094` (legacy). Syntax-malformed suffix still takes precedence
at `/` on both sides. Pin these literal outcomes independently and count actual
differences, without modifying the old admission helper or its test oracle.
Do not claim universal resource-path parity even on shared scalar policies.

Storage must be O(input bytes), including rejected deep/broad syntax, not one large
allocation per nesting level or quadratic ancestor strings while parsing. Use flat
index nodes, an iterative compact frame stack and one decoded arena. Source/decoded
bytes each cannot exceed input bytes. Node/frame counts each cannot exceed input
bytes+1; use checked size/index conversions and preserve these invariant bounds.
Document the actual node/frame sizes and capacity-growth bound. Report observed
logical sizes/capacities on worst-case tests without claiming whole-process peak
memory or zero allocations. Build pointer paths only after an actual resource error.
Resource BFS holds at most4096 queued records, not all unbounded diagnostic paths.

## Verification and independent comparison

Task1: reader/value/ownership/resource tests, functional RED after a successful
build, then GREEN on none and Telegram. Assert complete values/raw lexemes/decoded
bytes/child order and all published index/span invariants, not only bool results.
Cover all syntax/token transitions, both Unicode forms, embedded zeros, exact input
limit,4096/4097 values,32/33 container depths, decoded duplicate/pointer precedence,
malformed suffix overriding a deep/duplicate error, source lifetime and two-thread
independent reads. At the byte cap include deepest nesting, broad containers,
large strings and long number/exponent lexemes; no timing benchmark claim.

Task2: independent test-only pinned reader adapter in a Telegram-only differential
target plus deterministic generated cases. Compare full ordered values, decoded
strings/keys, booleans and raw number lexemes, and independent resource outcomes.
The adapter must not obtain expected values from readOwnJson or compute expected
failure paths with product helpers. Use the existing pinned parser only as a test
dependency, never link it to the reader or none target. Explicit policy tests pin
the known numeric/surrogate differences; do not hide those rows as skipped parity.
Keep same-policy comparisons and deliberate policy differences separately counted.
Retain reproducible seeds, input bytes/minimal differences and full first-error
details for failures. Finite generated coverage is not exhaustive fuzzing.

Root: fresh full none/Telegram/Win32 preview gates, Samsung configure boundary,
vendor/TGS integrity and source/include/link inspection showing own reader has no
vendor dependency. Existing old admission/input tests unchanged and pass. Independent
whole-stage review and report must state that old route is still vendor-dependent.

## Following connection, not included here

After reader acceptance, separately design first-party native admission using the
existing exact-decimal grammar once, own authored model/IDs/resources, and decouple
emission from C's reference certificate. The connection must specify which numeric/
Unicode contract and empty-ancestor diagnostic paths its own API uses; do not
silently switch existing APIs. Then add
reference-neutral timing, native pixel gates and static isolated Avelabs integration.

## Self-review and approval

No route activation is implied by compiling Formats in none. The scope is the
read/own/validate boundary; no duplicate exact-decimal or ellipse grammar. Whole
syntax versus bounded resource precedence is explicit, as are both deliberate
legacy scalar-policy differences. Private const ownership, indexed storage and
independent expected output have concrete test requirements. Controller approves
this written spec under delegated design authority; D is accepted at85a4c34;
next action is the detailed plan, not immediate product implementation.
