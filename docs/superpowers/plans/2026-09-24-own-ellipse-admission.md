# Part26F Own Ellipse Admission Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Consume accepted first-party JSON documents into the existing fully owned ellipse descriptor without a pinned-parser dependency on that own path, while preserving old admission behavior.

**Architecture:** One private compiled exact-decimal/grammar/materializer core consumes at most4096 borrowed ordered values. The Telegram legacy frontend retains its parsing/resources/raw-event gates; a separate all-variant own-document adapter calls the same core. No Runtime route or host activation.

**Tech Stack:** Existing C++20, MSVC2022, CMake/Ninja/CTest, Formats own reader and existing TGS transport; no new dependency.

**Spec:** docs/superpowers/specs/2026-09-24-own-ellipse-admission-design.md (controller-approved009dd2e).

## Global Constraints

- Direct main, small scoped commits, ordinary push after verification/review; no force, rebase, history rewrite. One product writer. Preserve raw/SDD.
- No vendor, dependency/license/notice/fixture/golden edits, install-policy changes, public fallback, Player, Direct2D ownership, ANGLE/backend coverage or Runtime.cpp route changes.
- No dependency installs, Windows/settings/security changes, automation, external publication or UI actions. All new interfaces stay private/not installed.
- No UI writes/builds/GUI or UI/out recreation in F; preserve accepted build/Release EXE and Motion Lab's default-OFF, no-install, experimental reference CPU status.
- Existing NativeEllipseAdmission.hpp and NativeEllipseInput.hpp remain unchanged. Existing legacy admission/input tests remain unchanged as regression evidence.
- No copy of vendor parsing or numeric eligibility, reserialization, own-to-old fallback, duplicated grammar, general DOM framework or mutable own-document builder.
- F does not claim full Lottie support, reference-free playback, native GPU rendering, universal legacy scalar compatibility, TSan, zero allocations or measured speedup.

## Review Focus

- Legacy raw number views must outlive the shared-core call; test descriptors after destroying all source/reader/frontend temporaries (Tasks1/2).
- Immediate siblings can be noncontiguous in flat storage; test a valid nested table with descendants between siblings and independent full descriptor (Task1).
- Two shared-core adapters can agree incorrectly; use independent literal descriptor/code/path expectations, never normalize expected values with product code (Tasks1/2).
- Resource/syntax failures must beat schema, and old empty-ancestor/lone-surrogate/numeric quirks stay distinct; explicit multiple-error and policy rows (Tasks1/2).
- Own default document and concurrent calls must not crash or share state; two-thread literal outputs plus absent failure publication (Task2).

## Baseline, files and execution

Stage BASE7000dd0b7470a17fd21ad3515a44eb7f140adae9. E final product9212c03:
none32/32, Telegram76/76, preview70/70; vendor/TGS16, exact graphs and supplemental
MSVC98/155 header traces passed. E source is unchanged by F docs. Existing direct
main checkout is user-selected (.git equals common-dir); no worktree/dependency
setup. A short fresh legacy-focused baseline before Task1 establishes current
input behavior; do not repeat E's full suites merely for planning.

File map:

- src/runtime/NativeEllipseAdmissionCore.hpp/.cpp: borrowed table contract and sole compiled decimal/grammar/materializer.
- src/runtime/NativeEllipseAdmission.cpp: existing Telegram compatibility frontend, original parse/resource/capture failures preserved, table projection.
- src/runtime/OwnNativeEllipseAdmission.hpp/.cpp: own document adapter, no byte/audit overload or routing.
- tests/support/NativeEllipseAdmissionTestData.hpp: test-only exact literal descriptor, fixture mutations and own-reader-to-test-table projection; not an expected-value parser or normalizer.
- tests/native_ellipse_admission_core_tests.cpp: all-variant core independent functional/descriptor/order/lifetime tests.
- tests/native_ellipse_admission_compat_tests.cpp: Telegram-only legacy literal eligibility/precedence regression evidence.
- tests/own_native_ellipse_admission_tests.cpp: all-variant own composition/descriptor/resources/lifetime/isolation.
- tests/own_native_ellipse_admission_differential_tests.cpp: Telegram-only supplementary old/own comparison plus independent policy expectations.
- CMakeLists.txt: unconditional core/own sources, private Formats include, four narrowly gated targets. No install/route/dependency policy edits.
- Root alone owns spec/plan/STATE/durable ledger, review orchestration and final gates; workers write their task reports/raw evidence only.

Commands use installed VsDevCmd x64, no shell-policy override:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
cmake --preset windows-msvc-direct2d
cmake --build --preset windows-msvc-direct2d --parallel 4
ctest --preset windows-msvc-direct2d --output-on-failure
```

Telegram preset windows-msvc-telegram-debug; graphics preset specifically
windows-msvc-win32-preview. All command/exits and RED/GREEN outputs redirected
under out/part26f/taskN, non-overwriting. Use apply_patch for authored runners.

---

### Task 1: Shared compiled admission core with preserved legacy frontend

**Files:** create NativeEllipseAdmissionCore.hpp/.cpp, support/NativeEllipseAdmissionTestData.hpp,
native_ellipse_admission_core_tests.cpp, native_ellipse_admission_compat_tests.cpp;
modify NativeEllipseAdmission.cpp and CMakeLists.txt only. Paths as file map above.

**Interfaces (namespace avemotion::runtime::detail):**

```cpp
using NativeEllipseValueId = std::uint32_t;
inline constexpr NativeEllipseValueId NativeEllipseNoValue = UINT32_MAX;
enum class NativeEllipseValueKind : std::uint8_t { Null, Boolean, Number, String, Object, Array };
struct NativeEllipseValue final {
    NativeEllipseValueKind kind = NativeEllipseValueKind::Null;
    bool hasKey = false;
    std::string_view key, scalar;
    NativeEllipseValueId firstChild = NativeEllipseNoValue;
    NativeEllipseValueId nextSibling = NativeEllipseNoValue;
    std::uint32_t childCount = 0;
};
[[nodiscard]] NativeEllipseAdmission evaluateNativeEllipseValues(
    std::span<const NativeEllipseValue> values,
    std::shared_ptr<const NativeEllipseInput>* output = nullptr);
```

Header includes existing private NativeEllipseInput.hpp and standard headers only.
Internal validated table: root0,1..4096 rows, every row reachable once, source-order
immediate links, scalar childCount0, root hasKey=false/nextSibling sentinel. Number
scalar is validated raw lexical bytes; String scalar is decoded bytes. Boolean
payload unnecessary: schema rejects Boolean kind. Failure clears a nonnull output;
null output performs audit without constructing a descriptor. Invalid internal
table is a programming error, not a new end-user JSON policy; do not expose it.

Test-only helper namespace avemotion::test exports:
`ellipseDecimal(bool,std::string,bool=false,std::string="0")`,
`expectedEllipseBaseline() -> NativeEllipseInput`,
`ellipseFixture() -> std::string`,
`replaceEllipseOnce(std::string,std::string_view,std::string_view) -> std::string`,
`staticEllipseFixture(std::string_view seed) -> std::string`,
`projectOwnForCore(const formats::detail::OwnJsonDocument&) -> std::vector<NativeEllipseValue>`.
Mutation helpers assert unique source fragments, never mutate committed fixture.
Projection is test infrastructure, not expected grammar output, and may copy the
accepted E node indexes only because every row is retained in original order.

- [ ] **Step 1: Preserve literal legacy baseline and establish new core functional RED.**

Run current old admission/input tests once under VsDevCmd (GREEN baseline, not RED).
Write helper's complete expected fixture descriptor without product normalization:

```cpp
NativeEllipseInput expectedEllipseBaseline() {
    auto d = [](bool n, std::string s, bool pn=false, std::string p="0") {
        return NativeEllipseDecimal{n,std::move(s),{pn,std::move(p)}};
    };
    NativeEllipseInput x;
    x.width=512; x.height=512; x.endFrame=61; x.frameRate=d(false,"6",false,"1");
    x.layerId=1; x.layerInFrame=0; x.layerOutFrame=61;
    x.layerTranslation={d(false,"256"),d(false,"256")};
    x.size={d(false,"12",false,"1"),d(false,"12",false,"1")};
    NativeEllipseAnimatedPosition p;
    p.firstFrame=0; p.lastFrame=60;
    p.start={d(true,"76"),d(false,"0")}; p.end={d(false,"76"),d(false,"0")};
    p.incoming={d(false,"667",true,"3"),d(false,"1")};
    p.outgoing={d(false,"333",true,"3"),d(false,"0")}; x.position=p;
    x.fillColor={d(false,"8",true,"2"),d(false,"72",true,"2"),d(false,"95",true,"2"),d(false,"1")};
    x.version="5.7.4"; x.name="AveMotion Telegram sticker profile fixture";
    x.layerName="Moving Circle"; x.groupName="Circle Group";
    x.ellipseName="Animated Ellipse"; x.fillName="Fill"; x.transformName="Transform";
    return x;
}
```

Core source initially compiles a rejection stub returning InvalidJson `/` and no
output. Wire core into unconditional Runtime and target
avemotion_native_ellipse_admission_core_tests / CTest
avemotion.runtime.native_ellipse_admission_core outside vendor guards, linking
AveMotion::Runtime with private src/runtime and src/formats includes and fixture
directory definition. Test parses existing telegram_sticker_basic.json using E,
projects validated test table and expects accepted full independently literal input:

```cpp
const auto json=ellipseFixture();
const auto doc=readOwnJson(json);
require(static_cast<bool>(doc), "core fixture JSON read");
const auto table=projectOwnForCore(*doc.document);
std::shared_ptr<const NativeEllipseInput> input;
const auto admitted=evaluateNativeEllipseValues(table,&input);
require(admitted.accepted() && input, "core emits accepted descriptor");
require(admitted.path.empty() && *input==expectedEllipseBaseline(), "complete literal descriptor");
```

Successful none configure/build must precede focused functional RED on accepted
descriptor, not a missing header/symbol/compiler. Preserve actual log and source
identity before extracting logic. Test table source lifetime must enclose call.

- [ ] **Step 2: Extract exact grammar/math/materialization once.**

Move first-party decimal functions and schema/materialize logic from the current
legacy file to core, retaining comparisons, constants, required/optional order
and short-circuit errors. No vendor code copied. Adapt value lookup to borrowed
records. Core owns the synchronous span; numbers normalized for every Number row
before root grammar. Keep source-local exact decimal types/functions, no public
numeric utility package. Old grammar implementation must not remain as a second
live copy. Immediate lookup must traverse sibling links, never firstChild+ordinal:

```cpp
const NativeEllipseValue* field(const NativeEllipseValue& object, std::string_view key) const {
    for (auto id=object.firstChild; id!=NativeEllipseNoValue; id=values_[id].nextSibling)
        if (values_[id].hasKey && values_[id].key==key) return &values_[id];
    return nullptr;
}
const NativeEllipseValue& element(const NativeEllipseValue& array, std::size_t ordinal) const {
    auto id=array.firstChild;
    while (ordinal-- != 0 && id!=NativeEllipseNoValue) id=values_[id].nextSibling;
    if (id==NativeEllipseNoValue) throw std::logic_error("invalid internal ellipse child index");
    return values_[id];
}
```

These are private validated-table operations, not permission to dereference bad
projector output. Root/size/sentinel preconditions and bounded projector traversals
must make invalid internal layouts fail clearly, not loop or silently publish.
Do not add a user-selectable second resource policy to core.

- [ ] **Step 3: Connect legacy frontend without eligibility/diagnostic changes.**

Keep old byte/NUL gate, pinned DOM parse flags, BFS resources/path helper EXACTLY
in ordering/behavior. Preserve raw SAX flags and every parse/kind/count mismatch
as InvalidJson `/`. After complete successful alignment, project all DOM values
into the table preserving keys, decoded string bytes and raw number event strings;
no float conversion. DOM and event vector own strings through the core call.
Never return views from the former local handler after its scope ends. Core now
normalizes after full successful alignment; this is documented internal factoring,
not identical interleaving, while all observable failures precede grammar as before.
Retain auditNativeEllipseInput(json) and decodeNativeEllipseInput(json) signatures.

Create Telegram-only avemotion_native_ellipse_admission_compat_tests / CTest
avemotion.runtime.native_ellipse_admission_compat. Its literal table asserts both
old auditor and decoder exact code/path, plus null descriptor on every rejection.
Pin these inputs, independently constructed paths and baseline mutation results:

| Input / baseline mutation | Literal old result |
| --- | --- |
| {} | UnsupportedStructure `/fr` |
| {"x":1,"y":2}, reversed y then x | UnsupportedField `/x`, respectively `/y` |
| {"fr":0} | UnsupportedStructure `/ip` |
| {"a/b~":0} | UnsupportedField `/a~1b~0` |
| {"a\u0000b":0} | UnsupportedField std::string{"/a\0b",4} |
| {"":{"a":1,"a":2}} | InvalidJson `/a` |
| {"":{"":1,"":2}} | InvalidJson `/` |
| root empty member containing32 nested arrays | ResourceLimit31 repetitions of `/0` |
| root empty member containing4095 nulls | ResourceLimit `/4094` |
| previous resource/duplicate plus trailing { | InvalidJson `/` |
| baseline fr=0e309 | InvalidJson `/` |
| baseline fr=1 followed by309 zeros then e-309 | InvalidJson `/` |
| baseline fr=1.7976931348623158e308 | InvalidJson `/` |
| baseline fr=1.79769313486231580e308 | UnsupportedValue `/fr` |
| baseline fr=240.00000000000001 | UnsupportedValue `/fr` |
| baseline first keyframe e=[76.00000000000001,0] | UnsupportedValue `/layers/0/shapes/0/it/0/p/k/1/s` |
| baseline nm="\uDC00" on layer | Accepted, literal name bytes ED B0 80, all other fields baseline |

Also accepted baseline, reordered root fields, fr=1e-9999, all7 absent/empty names,
and static position[-32768,32768] with full independently expected descriptor.
Store result before replacing source/retiring frontend; compare full fields later.

- [ ] **Step 4: Core topology and rejection/publication evidence.**

Verify projected valid fixture contains nested descendants between immediate
siblings; assert actual first-child ordinal+1 is not assumed next sibling. Add
deterministic table permutation remapping every link while keeping root0, child
order/key/value bytes; expected full descriptor unchanged. Retain one plain
literal assertion (root child key order / array nested-child indexes) independent
of remapping code. Mutation witness: exchange two shape siblings and assert
UnsupportedValue at `/layers/0/shapes/0/it/0/ty`, no descriptor. This must fail if
core ignores sibling order. Core audit-only nullptr returns Accepted with empty
path without entering materializer (inspect branch; no global allocation claim).
Prepopulate output with an independent descriptor before rejected core input and
verify it is cleared. Earlier successful owned descriptors remain unchanged.

- [ ] **Step 5: GREEN, regression, review report and scoped commit.**

Run focused core in none and core/compat/unchanged admission/input in Telegram.
Run full none suite once, and Telegram C/D tests matching
`native_ellipse_(binding|certificate|stream)` as regression. No full Telegram/
preview duplication; root owns those after Task2. Record actual counts/first
failures and source identities, exact old frontend extraction diff, private
includes, one grammar implementation and git diff --check. Commit only this
task's files as `refactor: share first-party ellipse admission core`, no push.
Report .superpowers/sdd/2026-09-24-own-ellipse-admission/task-1-report.md. No subagents.

### Task 2: Own document adapter and all-variant independent input evidence

**Files:** create src/runtime/OwnNativeEllipseAdmission.hpp/.cpp,
tests/own_native_ellipse_admission_tests.cpp,
tests/own_native_ellipse_admission_differential_tests.cpp; modify CMakeLists.txt;
extend test-only NativeEllipseAdmissionTestData.hpp only as needed. Core/legacy
changes only for a demonstrated defect, with separate retained functional RED.

**Interfaces:** consume Task1 NativeEllipseValue, NativeEllipseValueKind,
NativeEllipseValueId/NoValue and evaluateNativeEllipseValues as specified above;
consume immutable formats::detail::OwnJsonDocument and six OwnJsonKind values.
New private function in runtime::detail:

```cpp
[[nodiscard]] NativeEllipseInputResult decodeOwnNativeEllipseInput(
    const formats::detail::OwnJsonDocument& document);
```

Forward-declare document in own header, include NativeEllipseInput.hpp. Source
includes OwnJsonReader.hpp and core header; no pinned parser or old frontend call.
Default empty document safely returns InvalidJson `/`, null input. No new byte/
audit overload, public header, format-loader facade or Runtime route.

- [ ] **Step 1: Compiling adapter stub and functional RED.**

Add own source unconditionally to Runtime. Create all-variant target
avemotion_own_native_ellipse_admission_tests / CTest
avemotion.runtime.own_native_ellipse_admission linking Runtime/Threads only with
private src/runtime/src/formats includes and fixture definitions.
`AVEMOTION_FIXTURE_DIR` is repository tests/fixtures; `AVEMOTION_TGS_DIR` is
repository tests/compatibility/tgs. Keep existing warning helper/Threads discovery.
Test imports
Task1 literal helper, reads baseline via E, invokes own stub returning rejection,
expects accepted full descriptor. Capture successful-build functional RED on
`own document admits ellipse`, not configure/compile/link error.

```cpp
const auto parsed=readOwnJson(ellipseFixture());
require(static_cast<bool>(parsed), "own fixture read");
const auto input=decodeOwnNativeEllipseInput(*parsed.document);
require(static_cast<bool>(input), "own document admits ellipse");
require(input.admission.path.empty() && *input.input==expectedEllipseBaseline(), "own exact baseline");
```

- [ ] **Step 2: Borrowed projection and explicit test-caller composition.**

Retain all own document rows in original order so original child/sibling IDs map
unchanged. Switch every kind explicitly; preserve key presence and byte views;
Number/String require valueBytes, keyed rows memberName. Check empty root before
projection; successful reader count already≤4096. Impossible mutated internal
document is not exposed by API. Call core synchronously while document lives and
return owned result; no serialization/reparse/float normalization/fallback.

Local test helper composing reader then own admission must explicitly map errors:

```cpp
NativeEllipseInputResult readThenAdmit(std::string_view json) {
    auto parsed=readOwnJson(json);
    if (parsed) return decodeOwnNativeEllipseInput(*parsed.document);
    NativeEllipseInputResult out;
    switch(parsed.code) {
    case OwnJsonReadCode::InvalidJson: out.admission.code=NativeEllipseAdmissionCode::InvalidJson; break;
    case OwnJsonReadCode::ResourceLimit: out.admission.code=NativeEllipseAdmissionCode::ResourceLimit; break;
    case OwnJsonReadCode::Parsed: throw std::logic_error("parsed reader without document");
    }
    out.admission.path=parsed.path; return out;
}
```

This is a test caller seam, not production route or a claim that the document
adapter accepts byte input. No enum casting. Caller error paths remain E-owned.

- [ ] **Step 3: Independent descriptors, boundaries and lifetime.**

Use literal helper expectedEllipseBaseline and modify expected fields directly,
never ask either decoder or normalizer for expected data. Include baseline,
reordered fields, static[-32768,32768], fr=1e-9999 and1e-999999999999999999999999,
size[1e-9999,120], equivalent6000e-2, signed zero values/exponents, layer ID2147483647,
active interval10..20, translation[-32768,32768], size[16384,0.5], fill[0,1,0.4,1].
All7 optional names absent versus present empty and escaped UTF8 scalar names,
256-byte name accepted/257 rejected at literal path, embedded zero rejected.

Literal failures: own empty default document InvalidJson `/`; scalar root
InvalidType `/`; {} missing `/fr`; unknown/reordered/missing precedence from
Task1 table; malformed suffix `/`; bad grammar type/property and continuity at
literal paths. Every failed result has null input; every accepted result has empty
path and fully equal fields. Reader resource rows in local composition: B+1 before
syntax, valid baseline padded to exact1,048,576 accepted, literalNUL InvalidJson `/`,
33 arrays ResourceLimit32 `/0` segments, root empty member32 arrays ResourceLimit
extra slash plus31 `/0`,4095 nulls at root empty member ResourceLimit `//4094`.
Duplicate empty ancestor `//a`/`//`, slash/tilde/decoded-NUL keys; malformed suffix
overrides every resource/duplicate witness at `/`. No second resource implementation.

Destroy/overwrite source and reader document in a nested scope while retaining only
the descriptor, then compare every field to independent expected literal. Two
threads each64 iterations with names Thread A/Thread B and translation[11,22]/[33,44]
must yield distinct complete expected descriptors and repeat serially. No shared
test counter race, static cache or TSan claim. Existing telegram_sticker_basic.tgs
through Formats decode must yield full baseline descriptor; no new corpus/fixture.

- [ ] **Step 4: Supplementary Telegram differential with explicit policies.**

Create Telegram-only avemotion_own_native_ellipse_admission_differential_tests /
CTest avemotion.runtime.own_native_ellipse_admission_differential. Links Runtime,
private first-party headers only; no new vendor include. Complete comparator:
admission code, length-aware path, bool/publication and full descriptor equality.
Use common cases from Step3 plus literal expected values on representative cases.
Mutate a copied observation's path, name, decimal power, variant or publication
and prove comparator notices; equal old/new alone is not independent grammar truth.

Pin own/old literal pairs (all reject without descriptor unless Accepted):

| Input/baseline mutation | Old | Own composition |
| --- | --- | --- |
| fr=0e309 | InvalidJson `/` | UnsupportedValue `/fr` |
| root ip=0e999999999999999999999999 | InvalidJson `/` | Accepted full baseline |
| fr=1 +309zeros+e-309 | InvalidJson `/` | Accepted rate+1e0/rest baseline |
| fr=1.7976931348623158e308 | InvalidJson `/` | UnsupportedValue `/fr` |
| fr=1.79769313486231580e308 | UnsupportedValue `/fr` | Same |
| fr=1e-9999 | Accepted full rate+1e-9999 | Same |
| layer nm=escaped DC00 | Accepted full baseline/name EDB080 | InvalidJson `/` |
| {"x":1e309} | InvalidJson `/` | UnsupportedField `/x` |
| {"x":1,"x":2,"y":compensated310digit token} | InvalidJson `/` | InvalidJson `/x` |
| {"":{"a":1,"a":2}} | InvalidJson `/a` | InvalidJson `//a` |
| root empty member32 nested arrays | ResourceLimit31 `/0` | ResourceLimit extra slash +31 `/0` |
| root empty member4095 nulls | ResourceLimit `/4094` | ResourceLimit `//4094` |
| all valid JSON witnesses +trailing { | InvalidJson `/` | InvalidJson `/` |

Counts separate actual common/deliberate cases; no skipped mismatch or automatic
whitelist. For numericaccepted cases assert literal normalized fields, not just
status. Include old audit-only alongside decode outcomes where applicable.

- [ ] **Step 5: Focused GREEN, full none, report and scoped commit.**

Build/run core and own tests under none; all four F targets plus unchanged legacy
admission/input and C/D regression under Telegram. Run full none once; root final
full Telegram/preview only after independent task review. Capture actual commands,
counts/failures/logs and hash identities; inspect no vendor headers/oldentrycalls
on own path, no grammar copy or output lifetime leakage. git diff --check, commit
only task files as `feat: admit own JSON documents into native ellipse input`, no
push. Full report .superpowers/sdd/2026-09-24-own-ellipse-admission/task-2-report.md.
No worker subagents, no UI/build/settings changes.

## Controller final gates and handoff

- [ ] Independent task reviews, any bounded fixes, updated ledger/STATE and guarded ordinary main pushes.
- [ ] Fresh full MSVC none/Telegram/Win32-preview configure/build/CTest at stable final product HEAD, zero skipped required tests; existing graphics gates explicitly required.
- [ ] Samsung configure graph/registration only, both vendor trees/corpus and TGS16 integrity; no Samsung all-green claim.
- [ ] Actual core/own source include traces and none link closure, source ownership, private installation and old-header/test/protected-file immutability. Telegram Runtime remains reference-linked intentionally.
- [ ] Whole-stage independent review over7000dd0..HEAD, at most one combined final wave/scoped review, report exact commands/counts/limitations and all rulings/costs.
- [ ] After F acceptance, separate own-model/ID/resource/certificate design, then native timeline/pixels and isolated static Avelabs acceptance. No automation resumed.

## Preflight and execution decision

Spec/core extraction+legacy preservation ->Task1; own document projection and
independent composition/lifetime/policy ->Task2; platform/provenance/privacy ->root.
Both tasks share the exact descriptor unchanged and table/core signatures above;
Task2 cannot begin before reviewed Task1. Same helper factory gives independently
literal expected values, not parser-computed expected outputs. All five Review
Focus items have named tests. Scope avoids general JSON APIs/unused numeric package.
Controller approves the written plan under delegated authority and selects SDD,
one writer and independent task/final reviews. Not a claim of unseen user review.
