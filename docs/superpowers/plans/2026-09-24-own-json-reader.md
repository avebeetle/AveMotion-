# Own JSON Reader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Supply a privately testable, bounded first-party JSON value reader without rlottie, preserving exact numeric lexemes and owning all published data.

**Architecture:** A private Formats reader parses a complete document iteratively into indexed owned storage, then applies breadth-first duplicate/resource checks. Existing admission and animation routes stay unchanged. Unit tests run in none; a Telegram-only independent legacy adapter compares common-policy results and explicitly records intentional scalar-policy differences.

**Tech Stack:** C++20, installed MSVC x64, existing CMake/CTest/Formats/TGS; no new dependencies.

**Spec:** docs/superpowers/specs/2026-09-24-own-json-reader-design.md

## Global Constraints

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

## Review Focus

1. A malformed suffix after an overdeep/duplicate subtree must remain InvalidJson `/`, not an early resource path (Task1 precedence tests).
2. Empty/NUL-containing decoded keys and Unicode aliases must remain length-aware and duplicate by decoded bytes, without strlen or early map deduplication (Task1 key tests; Task2 independent observations).
3. Source/document/vector moves must not leave borrowed spans or pointers into reallocated storage; invalid lookup must fail safely (Task1 ownership/index tests).
4. A full1MiB nested/broad input must not use input-driven recursion or quadratic path storage before rejection; counters must measure actual storage (Task1 cap-stress cases).
5. Numerically equivalent spellings can have different legacy eligibility; no implicit binary conversion or blanket parity assertion may creep into own reading (Task1 lexemes; Task2 explicit difference matrix).

## File map and verification

- src/formats/OwnJsonReader.hpp: private immutable document/result/value/index/statistics contract.
- src/formats/OwnJsonReader.cpp: own iterative syntax/Unicode/number reader and post-parse breadth-first resource gate.
- tests/own_json_reader_tests.cpp: direct contract, full values, lifetime, limits, storage and concurrency.
- tests/support/OwnJsonLegacyOracle.hpp: independent test-only pinned parse/resource/value observation.
- tests/own_json_reader_differential_tests.cpp: full-value common-policy differential/generated cases and deliberate differences.
- CMakeLists.txt: own source in Formats, unit target all variants, differential target Telegram only.
- Controller owns docs/STATE/ledgers. Raw out/part26e/taskN; task-N-report.md in this plan's SDD workspace. No workers edit controller documents or spawn agents.

All commands run after the installed VsDevCmd -arch=x64. Write .cmd runners with
apply_patch, separate RED/GREEN/full logs and Git identities. Do not modify shell
policy. Exact normal gates:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake --preset windows-msvc-direct2d
cmake --build --preset windows-msvc-direct2d --parallel 4
ctest --preset windows-msvc-direct2d --output-on-failure
```

Use windows-msvc-telegram-debug for reference comparison. Root owns final preview/
vendor/corpus/graphs and whole-stage review, not duplicate worker final suites.

---

### Task 1: Owned bounded reader and direct contract tests

**Files:**
- Create src/formats/OwnJsonReader.hpp and OwnJsonReader.cpp.
- Create tests/own_json_reader_tests.cpp.
- Modify CMakeLists.txt only Formats source and unit test registration.

**Interfaces:**
- Namespace avemotion::formats::detail; no Runtime or public header dependencies.
- `using OwnJsonNodeId = std::uint32_t;` and `inline constexpr OwnJsonNodeId OwnJsonNoNode = UINT32_MAX;`.
- `enum class OwnJsonKind : std::uint8_t { Null, Boolean, Number, String, Object, Array };`.
- `OwnJsonNode` fields: kind; bool boolean=false, hasKey=false; OwnJsonNodeId firstChild=OwnJsonNoNode, nextSibling=OwnJsonNoNode; std::uint32_t childCount=0, keyOffset=0, keyLength=0, dataOffset=0, dataLength=0. Key/string spans address decoded storage; Number span addresses owned source.
- `OwnJsonReadStatistics`: size_t inputBytes,sourceBytes,decodedBytes,nodeCount,nodeCapacity,framePeak,frameCapacity,nodeSizeBytes,frameSizeBytes,resourceQueueCount, all initialized0.
- `OwnJsonDocument` has read-only `nodes() -> std::span<const OwnJsonNode>`, `node(OwnJsonNodeId) -> const OwnJsonNode*` (nullptr if invalid), `source() -> std::string_view`, `valueBytes(OwnJsonNodeId) -> std::optional<std::string_view>` (only Number/String), `memberName(OwnJsonNodeId) -> std::optional<std::string_view>` (only hasKey). No mutating public method. Own strings/vector privately, using a source-local friend builder if needed.
- `enum class OwnJsonReadCode { Parsed, InvalidJson, ResourceLimit };`.
- `OwnJsonReadResult` has code defaultInvalidJson, owned std::string path, shared_ptr<const OwnJsonDocument> document, OwnJsonReadStatistics statistics; explicit bool only Parsed+document.
- `readOwnJson(std::string_view bytes) -> OwnJsonReadResult`.
- Test target avemotion_own_json_reader_tests, CTest avemotion.formats.own_json_reader; link only AveMotion::Formats and Threads::Threads, private src/formats include, existing warnings/fixture-dir definition. Use existing CMake Threads detection, no duplicate dependency install.

- [ ] **Step 1: Declare interface, wire target and capture functional RED.**

Add source to existing Formats static library for every variant. Target belongs
under AVEMOTION_BUILD_TESTS, outside reference guards. Start a compiling reader
stub returning InvalidJson `/`; do not call another parser. Initial test:

```cpp
const std::string input = R"({"a":[true,null,-0.00e+12],"s":"A\u00e9"})";
auto parsed = readOwnJson(input);
require(static_cast<bool>(parsed), "own JSON document parsed");
require(parsed.path.empty(), "success path empty");
require(parsed.document->node(0)->kind == OwnJsonKind::Object, "object root");
require(parsed.document->source() == input, "exact owned source");
```

Use a named `const std::string input` for the shown bytes. Successful configure/
link, then focused none CTest must fail `own JSON document parsed`, not a missing
symbol/header failure. Preserve commands/output in out/part26e/task1/red-*.

- [ ] **Step 2: Implement iterative syntax and owned indexed values.**

Precheck B>1,048,576 before allocation as ResourceLimit `/`; empty/literalNUL as
InvalidJson `/`. Copy source once. Parse using a flat stack of container frames
and numeric indexes into a node arena, not recursive functions/destructors. A
frame tracks container index, last child index, expected grammar state and pending
key span. Object states: first-key-or-end, key-required-after-comma, colon,
value, comma-or-end. Array states: first-value-or-end, value-required-after-comma,
comma-or-end. Updating parent state happens before pushing child; never retain a
reference to a vector element across push/reserve. Root completion permits only
space/tab/CR/LF then EOF. No publication until all gates pass.

```cpp
// Link child by stable indexes; no reference lives across arena growth.
const OwnJsonNodeId child = appendNode(kind);
if (lastChild == OwnJsonNoNode) nodes[container].firstChild = child;
else nodes[lastChild].nextSibling = child;
++nodes[container].childCount;
// Update lastChild in the indexed parent frame before any nested frame push.
```

Independently author scalar scanners: exact true/false/null; number grammar
`-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?`. Preserve offset/length, never
compute double or exponent power. Following invalid characters fail document
syntax instead of accepting a prefix. Strings decode quote/slash/backslash and
b/f/n/r/t/u escapes, rawUTF8 scalar sequences, valid surrogate pairs. Reject lone
surrogates, overlong/raw surrogate/out-of-range/truncated UTF8 and raw controls.
Accumulate decoded bytes into one owned arena; distinguish empty key via hasKey.
Offsets/counts use checked uint32 conversions; input cap keeps bounds representable.

Keep sizes/capacities and frame high-water mark in actual statistics on success or
failure. Node/frame logical count <=B+1, decoded bytes <=B. Use geometric bounded
reserve growth (small seed, checked multiplication, cap derived from B+1); no per-
node maps/paths and no allocation proportional to exponent numeric value. Document
sizeof node/frame and observed capacities. Normal bad_alloc is not syntax failure.

- [ ] **Step 3: Add post-syntax resource/duplicate gate with precedence RED/GREEN.**

After complete syntax only, BFS queue stores index,parentQueueIndex,depth and
arrayIndex/name association. Root depth1. Dequeue containerdepth>32 ->ResourceLimit
at that node. Within each object inspect decoded keys in source order, compare
length-aware against prior keys BEFORE child enqueue/count check. Duplicate ->
InvalidJson at escaped child pointer. Before a child enqueue when queue.size()==4096,
ResourceLimit at that child. Array children follow order. Root itself counts.
Build paths lazily on failure from at most32/33 ancestors. Never recursively walk
the full tree just to report a rejected deep node. Root bad syntax always `/`.

```cpp
expect("{\"a\":1,\"\\u0061\":2}", OwnJsonReadCode::InvalidJson, "/a");
expect("{\"a/b~c\":1,\"a\\/b~c\":2}", OwnJsonReadCode::InvalidJson, "/a~1b~0c");
expect("{\"a\":1,\"a\":2} {", OwnJsonReadCode::InvalidJson, "/");
expect(nestedArrays(32), OwnJsonReadCode::Parsed, "");
expect(nestedArrays(33), OwnJsonReadCode::ResourceLimit, repeatPath(32));
expect(arrayOfNulls(4095), OwnJsonReadCode::Parsed, "");
expect(arrayOfNulls(4096), OwnJsonReadCode::ResourceLimit, "/4095");
expect(nestedArrays(33) + "{", OwnJsonReadCode::InvalidJson, "/");
```

Helpers construct `n` '[' + '0' + `n` ']' and paths `/0` repeated32. Add resource-
versus-duplicate queue-order competitors, exact empty-key `/`, and duplicate
decoded-NUL key with expected std::string{"/a\0b",4}; inputs use escapedNUL.
Verify no document on every failure, and counted queue<=4096. Record a focused
functional failure before implementing the missing resource logic.

- [ ] **Step 4: Full scalar, ownership, capacity and concurrency matrix.**

Direct table: all6 kinds; booleans both; empty/nonempty arrays/objects; all escapes;
ASCII and 2/3/4-byte UTF8; escaped pair U+1F600; U+0000 value/key; slash/tilde keys;
all grammar states' prematureEOF and wrong-separator variants; BOM/comments/trailing
commas/NaN/Infinity/leadingzero/plus/badfraction/badexponent. Invalid raw bytes:
C0AF,E282,EDA080,EDB080,F4908080,FF in both key and value. Reject escapedD800,DC00,
D800+0041 and badhex/unknownescape. `"0e309"`, `"1e-999999999999999999999999"`,
`"-0.00e+00012"` as unquoted number documents parse with exact raw bytes retained.

Exercise source overwrite/release and copied document handles. Hold an early
child index while later large parsing grows arenas; verify every final node's
kind, key/value bytes, child order/count, spans and sibling chains (acyclic/in-range,
each nonroot has one parent, no lost nodes). Invalidnode lookup/no value for Bool
must return nullptr/nullopt, not arbitrary string or out-of-bounds access.

At B=1,048,576: valid smallroot+whitespace; broad zero array with enough elements
to exceed4096; nested arrays around0 plus padding to cap (depth524287); one large
string and one very long valid exponent token. For deep and broad valid syntax
expect resource paths; append/replace final needed delimiter with malformed byte
while remaining at cap and require syntax `/`. B+1 invalid content still gives
ResourceLimit `/` before syntax. Print source/decoded/node/frame/queue statistics
for each named stress row and assert logical invariant bounds. No megabyte raw
dump in normal output; failures save exact input under named raw path.

Two threads independently parse128 small constructed docs each; compare complete
owned values against expected literals and repeat serial results, with no shared
mutable cache/global configuration. This is not a TSan claim.

Read committed telegram_sticker_basic.json and repeater_content_group.json plus
decoded tgs/repeater_content_group.tgs; full document equality of JSON/TGS decoded
payload using the same named owned observations. Reuse Formats::decodeTgs APIs,
never modify fixtures/generator. No Lottie feature claim from JSON parse success.

- [ ] **Step 5: GREEN, regression suites, self-review, scoped commit/report.**

Run focused target both windows-msvc-direct2d and windows-msvc-telegram-debug.
Run full none suite and existing Telegram tests matching
`native_ellipse_(admission|input)|formats\.own_json_reader|formats\.tgs` once.
Root will run complete Telegram/preview after Task2. Inspect source dependency path
and private header placement, git diff --check. Commit listed files only as
`feat: add bounded first-party JSON reader`; no push. Report exactRED/GREEN commands,
outputs/counts, storage sizes/capacities, earliest failure/root cause, interfaces
and limitations. Controller reviews before Task2; no worker subagents.

### Task 2: Independent differential and intentional-policy evidence

**Files:**
- Create tests/support/OwnJsonLegacyOracle.hpp.
- Create tests/own_json_reader_differential_tests.cpp.
- Modify CMakeLists.txt only Telegram differential test registration.
- Amend Task1 source/tests only for demonstrated functional defects with retained RED.

**Interfaces:**
- Consumes Task1 readOwnJson/result/document/node/statistics API as named above.
- Test namespace avemotion::test. `OwnJsonObservation` value-owned code/path plus ordered `OwnJsonObservedValue` rows: kind,boolean,hasKey,key,valueBytes,parentOrdinal,childCount. Defaulted equality compares all fields; raw numeric lexemes are strings, not doubles.
- `observeOwnJson(const formats::detail::OwnJsonReadResult&) -> OwnJsonObservation` walks candidate indexes; `observeLegacyJson(std::string_view) -> OwnJsonObservation` independently parses pinned source. Expected side must not call own reader, product helpers or candidate node builders.
- Target avemotion_own_json_reader_differential_tests, CTest avemotion.formats.own_json_reader_differential; Telegram guard only, links AveMotion::Formats and rlottie::rlottie only for existing private RapidJSON header availability; includes private src/formats and required Telegram internal Lottie include. No vendor linkage in Formats or none unit target.

- [ ] **Step 1: Independent observation and functional comparison RED.**

Legacy adapter uses existing RapidJSON Document with iterative+validateEncoding
flags for full-document eligibility, independent BFS resource/duplicate checks,
then raw-number-as-strings SAX events aligned to source-order DOM values. Input
byte/NUL checks match reader's initial gates. Its observations descend every value,
decoded key/string and exact number token; no own-helper computation of expected
paths or values. Keep expected paths length-aware. Parsed observations use source-
order preorder rows with parentOrdinal and childCount, not pointer addresses.

Start comparison test with deliberate empty observations and assert real source
has expected values, showing a functional failure after successful compile/link.
Then correct the observation path; mutation witnesses of populated observations
must detect changed key, string, kind, bool, numeric spelling, parent/order/count,
code/path and missing value. Direct literal expected assertions guard two broken
observers agreeing. These are test-only helpers, no new product schema.

```cpp
const std::string json = R"({"k":[true,12.00e-1,"\u00e9"]})";
const auto actual = observeOwnJson(readOwnJson(json));
const auto expected = observeLegacyJson(json);
require(actual == expected, "complete ordinary-policy observations agree");
require(actual.values.size() == 5, "root array and three scalar rows retained");
auto changed = actual;
changed.values.back().valueBytes = "different";
require(!(changed == expected), "decoded-value mutation detected");
```

- [ ] **Step 2: Pin known deliberate scalar-policy differences without skips.**

Build these16 tokens in test code (do not depend on out files at runtime), each as standalone number and
`{"x":TOKEN}`. Legacy parses tokens1,2,4,6,7,10,11,12,16; rejects3,5,8,9,13,14,15
as InvalidJson `/`. Own reader parses all32 documents and retains exact lexemes.
`Z(n)` below means n literal zero digits concatenated into the token:

| ID | Token construction |
| --- | --- |
|1|0.0e309|
|2|-0.0e309|
|3|1.0e309|
|4|0.00e310|
|5|0.00e311|
|6|`0.` +Z(400)+ `1e401`|
|7|`0.` +Z(400)+ `1e402`|
|8|`1` +Z(309)+ `e-309`|
|9|`1` +Z(309)+ `e-616`|
|10|`1` +Z(309)+ `e-617`|
|11|`-1` +Z(309)+ `e-617`|
|12|`1` +Z(308)+ `e-308`|
|13|1.7976931348623158e308|
|14|-1.7976931348623158e308|
|15|17976931348623158e292|
|16|1.79769313486231580e308|

Legacy Parse means reader
syntax/resources, not old ellipse grammar acceptance. Do not use auditUnsupportedValue
as parserInvalidJson. Both sides also test0e309 (ownParsed/legacyInvalidJson),
0e308 (bothParsed), huge negative exponent (bothParsed).

Unicode: escapedDC00 in value/key is ownInvalidJson `/`, legacyParsed; duplicate
escapedDC00 keys are ownInvalidJson `/`, legacyInvalidJson pointerbytes2FEDB080.
RawEDB080 rejects both; validpairD83DDE00 gives equal decodedF09F9880. Whole syntax
malformed suffix rejects both `/`. Token8 with earlier duplicate x gives ownInvalidJson
`/x`, legacyInvalidJson `/`; token10 with duplicate x gives both `/x`. These rows are
explicit expected policy differences, not relaxed generic comparison or filtered
failures. Print same-policy and intentional-difference counts separately.

- [ ] **Step 3: Deterministic common-policy generation and byte mutations.**

Own small deterministic xorshift32 PRNG with fixed seed0x26E2026U; generator emits
512 docs of depth0..6 with0..5 children and unique keys, all6 kinds, safe exact
numeric spellings (integers -10000..10000, fractions and exponents -8..8), escaped
Unicode scalar strings/controls and shuffled key order. Bound generator to96 value
nodes and short strings<=32 decoded bytes, choosing a scalar when node budget is
exhausted; never truncate a completed JSON document. Keep each input<=8192bytes
and use loop counters, not wall time. Compare complete observations and every
candidate published index/span invariant. Replay same seeds reproduces same bytes.

For each valid doc, produce one deterministic single-byte deletion and one
replacement of a structural delimiter by '?' at a located delimiter (if scalar,
append '?');1024 mutations. Some deletions remain valid: compare full outcomes,
never assume all malformed. Numeric generation deliberately stays far from
legacy conversion boundaries; any unexpected policy discrepancy is retained and
diagnosed, not auto-whitelisted. Print seed/case/category and first differing row/
field; save exact failure bytes/rawhex in out/part26e/task2/failures without deleting
other files. No external corpus/fuzzer dependency. Expected counts512+1024.

Run named same-policy resource/precedence boundaries independently:32/33 depth,
4095/4096 nullchildren, empty/NUL/escaped duplicate keys, multiple duplicates in
different depths/source orders, deep/resource plus malformed suffix. Test the
committed fixtures telegram_sticker_basic.json,repeater_content_group.json,
primitive_geometry.json,trim_path_geometry.json and TGS decodedrepeater payload.
No skipping a resource result; exact code/path compares even on rejection, with
the later whole-stage addendum's explicitly inventoried empty-ancestor path cases
asserting separate literal own/legacy expectations rather than false equality.

- [ ] **Step 4: GREEN and reviewed handoff.**

Build differential/own unit targets in Telegram and run focused both, then one
full Telegram suite. Run focused own unit in none after any product fix; retain
original failure before its fix, and report if a new production change occurs.
git diff --check, self-review expected-output independence and exactpolicy cases,
scoped commit `test: compare own JSON reader with pinned parser policies`.
No push; controller owns review and finalgates. Report commands/output, executed
counts, actualdifferences, dependencyboundary and all limitations.

### Task 3: One combined whole-stage fix wave

This is the sole final fix wave, not a new product feature. FIX_BASE is
88ee923db546c2fecdd53cce53eaedb77e1e9d0c. Read the full findings in
.superpowers/sdd/2026-09-24-own-json-reader/final-review.md and the spec's
empty-ancestor execution addendum; durable Ruling7 resolves the contract.

**Files:** modify only tests/own_json_reader_tests.cpp and
tests/own_json_reader_differential_tests.cpp. Keep each small graph checker local
to its test translation unit; no new helper framework or product/header changes.
Controller owns spec/plan/STATE/ledgers and final full gates.

- [ ] **Step 1: Pin the empty-ancestor diagnostic distinction.**

Retain a focused functional RED for the former shared-policy equality assumption
using `{"":{"a":1,"a":2}}`; it must compile and expose own `//a` versus old `/a`.
Then update test expectations to the controller-approved separate policy, not the
reader, old admission, or oracle. Add literal expectations in direct and
differential tests for duplicate `a` under an empty ancestor, duplicate empty
name under an empty ancestor, root empty member containing32 nested arrays, and
root empty member containing4095 null children. Exact codes/paths are in the spec
addendum. Include the nonempty-prefix `/outer//a` control and malformed-suffix
syntax `/` controls. Published document must be absent on every rejected result.
Count deliberate differences separately; don't change the512+1024 all-byte
generator, seed, case184, existing scalar witnesses, fixtures, or oracle behavior.

- [ ] **Step 2: Prove arena reachability in both invariant helpers.**

Use small local test-only predicates over span<const OwnJsonNode> (no product
mutation API), with an iterative traversal from root index0, bounded index/visit
checks, and total visited exactly equal to the arena size. Preserve existing
parent-count, sibling, span, kind/value checks. Explicitly reject repeated visits,
unreachable nodes, out-of-range edges and cycles without hanging. Build synthetic
POD node vectors: a valid root tree control and root plus a disconnected two-node
cycle whose nonroot parent counts all equal1. Add negative assertions first against
the former parent-count-only check, capture successful-build functional RED, then
strengthen the checker and use it on every published document. Exercise the helper
in both test translation units; do not loosen any existing assertion.

- [ ] **Step 3: Focused GREEN, report and scoped commit.**

Use installed VsDevCmd x64, existing none and Telegram build trees, focused direct
test in none and both reader tests in Telegram. Keep actual redirected RED/GREEN
outputs under out/part26e/final-fix with commands/exits and source identity. Root
runs final complete none/Telegram/preview suites after writer quiescence; readonly
scoped review can overlap those stable-input gates, acceptance requires both. Do
not duplicate them. git diff --check and scoped commit the two test files only,
`test: pin reader path policy and prove arena reachability`; no push. Report
at .superpowers/sdd/2026-09-24-own-json-reader/task-3-report.md, including complete
findings addressed, chronology, counts and limitations. No subagents.

## Controller final gates and continuation

- [ ] Independent task reviews/fixes, scoped commits and ordinary push with exact remote guards.
- [ ] Fresh full none/Telegram/Win32 preview configure/build/CTest, all-vendor/TGS16 integrity.
- [ ] Inspect fresh compiler/include/link graphs: OwnJsonReader only Formats, no vendor dependency in its command or none test; differential only Telegram; existing D still excluded from none/Samsung.
- [ ] Whole-stage review overD final85a4c34..HEAD, report raw matrix/capacities and policy differences, unchanged legacy admission/API and no native-loading claim.
- [ ] Continue own-admission/model connection design, timeline/pixel/isolated static Avelabs path without resuming schedules.

## Self-review and execution choice

Spec scope/ownership/scalarpolicy/resources ->Task1, independent known-policy and
generated evidence ->Task2; private/platform/oldroute/provenance ->root. Five review
focuses have named assertions. Same-policy versus deliberate differences remain
separate; old admission does not consume the new reader. Sources/targets are placed
in their actual existing libraries, with no new module or reverse dependencies.
All referenced document/index names match Task1. Controller approved this written
plan under delegated authority and selects SDD with one writer, fresh task reviews
and whole-stage review. No claim of user review of unseen artifacts.
