# Owned Native Ellipse Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Retain every admitted one-ellipse JSON value in a private, exact, owned descriptor without changing admission or playback.

**Architecture:** Share the existing admission parse/audit and exact metadata. Materialize only after success from that same DOM, publish `shared_ptr<const NativeEllipseInput>`, and keep audit-only calls allocation-equivalent with respect to descriptor construction. No production call site selects this path.

**Tech Stack:** C++20, MSVC, CMake/CTest, existing Telegram-private RapidJSON; PowerShell and installed Python for checks.

**Spec:** `docs/superpowers/specs/2026-09-24-owned-native-ellipse-input-design.md`

## Global Constraints

- Work directly in main; ordinary scoped commits/push only, no history rewriting.
- Keep Part25I admission codes, paths, accepted grammar and resource limits unchanged.
- Preserve exact decimal values; no floating-point conversion or new precision/exponent cap.
- Keep the descriptor private and Telegram-only; the none/Samsung build must not acquire this parser.
- Do not change public Runtime APIs/fallback, Player threading, Direct2D ownership or ANGLE/backend coverage.
- Do not modify vendor sources, versions, patches, licenses/notices, fixtures or goldens.
- Do not install dependencies, change Windows settings, or write/build/run GUI in Avelabs-UI.
- Preserve raw evidence and SDD records; do not resume or create an automation.

## Review Focus

1. Decimal underflow/large exponent/equivalent tokens must retain exact canonical values, not become zero or rounded boundaries (Task 1 numeric tests).
2. Key-order and absent/empty/UTF-8 inert names must not shift fields or borrow source memory (Task 1 naming/lifetime tests).
3. A late rejection following a valid call must expose no stale/partial object and preserve code/path (Task 1 rejection tests).
4. Static versus animated position must preserve its tag and all times/easing, including a layer inactive at zero (Task 1 explicit field tests).
5. Simultaneous distinct inputs must not share mutable parse state or overwrite retained results (Task 1 concurrent ownership tests).

---

### Task 1: Exact owned descriptor and dedicated tests

**Files:**
- Create: `src/runtime/NativeEllipseInput.hpp` (private standard-C++ value/result contract).
- Modify: `src/runtime/NativeEllipseAdmission.cpp` (shared parse/audit with optional success-only extraction).
- Create: `tests/native_ellipse_input_tests.cpp` (functional and boundary assertions).
- Modify: `CMakeLists.txt` (one Telegram-only test next to native admission test).
- Modify: `tests/native_ellipse_admission_tests.cpp` only if needed to assert decoder code/path on the existing complete mutation matrix; do not weaken existing assertions.
- Report: task report and raw logs in this plan's SDD workspace / `out/part26b/`.

**Interfaces:**
- Consumes: `NativeEllipseAdmission auditNativeEllipseInput(std::string_view)` and the existing validated `ExactDecimal` metadata/`boundedInteger` helper.
- Produces: `NativeEllipseInputResult decodeNativeEllipseInput(std::string_view)` and all exact private value types/fields defined verbatim in the spec Data contract. Include that spec before coding; it is the single field/type authority.
- No Runtime, model, host or installed-public-header caller changes.

- [ ] **Step 1: Declare the private contract and write the functional RED.**

Use `<array>`, `<cstdint>`, `<memory>`, `<optional>`, `<string>`, `<string_view>`,
`<variant>` plus `NativeEllipseAdmission.hpp`. Value members default to zero;
the result's bool checks both acceptance and object presence. Default equality
on decimal, power, static/animated payload and complete input. Initially the
new decode function may return `{auditNativeEllipseInput(json), {}}`; this is
a deliberate test scaffold, not the implementation to keep. Existing audit
must keep working. Read the fixture using the existing test idiom and assert:

```cpp
const auto result = decodeNativeEllipseInput(readFixture("telegram_sticker_basic.json"));
require(result.admission.accepted(), "fixture remains admitted");
require(result.input != nullptr, "accepted input publishes owned descriptor");
require(result.input->width == 512 && result.input->height == 512, "dimensions");
require(result.input->endFrame == 61 && result.input->layerId == 1, "timeline/id");
const NativeEllipseDecimal sixty{false, "6", {false, "1"}};
require(result.input->frameRate == sixty, "exact rate");
const auto& motion = std::get<NativeEllipseAnimatedPosition>(result.input->position);
require(motion.firstFrame == 0 && motion.lastFrame == 60, "keyframe times");
const NativeEllipseDecimal minus76{true, "76", {false, "0"}};
const NativeEllipseDecimal incomingX{false, "667", {true, "3"}};
require(motion.start[0] == minus76 && motion.incoming[0] == incomingX, "motion fields");
```

Complete the baseline assertions with literal expected XY translation256/256,
size120/120, start[-76,0], end[76,0], outgoing[.333,0], incoming[.667,1],
fill[.08,.72,.95,1], interval[0,61), all seven names/version from the fixture.
Use a test-local exact literal factory or explicit structs, never call product
normalization to calculate expected numbers. Add CMake executable
`avemotion_native_ellipse_input_tests`, private include `src/runtime`, Runtime
link, fixture/TGS directory macros, warnings, and CTest name
`avemotion.runtime.native_ellipse_input` in the Telegram-only block.

- [ ] **Step 2: Run and retain the expected assertion failure.**

In a process initialized by existing VS2022 BuildTools `VsDevCmd.bat -arch=x64`:

```text
cmake --preset windows-msvc-telegram-debug
cmake --build --preset windows-msvc-telegram-debug --target avemotion_native_ellipse_input_tests avemotion_native_ellipse_admission_tests --parallel 4
ctest --preset windows-msvc-telegram-debug -R avemotion.runtime.native_ellipse_input --output-on-failure
```

Expected RED: executable runs, fixture admission passes, descriptor-nonnull
assertion fails. Compile/link errors do not satisfy RED. Keep command/output.

- [ ] **Step 3: Share validation and materialize only after complete success.**

Extract the current bottom-level parse function into one internal helper with
an optional output pointer. Both public-private entry points call it. Keep
resource/error ordering exactly as today. Inside the Auditor, expose an
internal run parameter for optional descriptor output and after rootObject
success copy exact metadata, bounded structural integers, vectors and names:

```cpp
// Structural sketch; named field readers remain small private Auditor helpers.
if (result_.accepted() && output != nullptr) {
    *output = std::make_shared<const NativeEllipseInput>(materialize(root));
}
return result_;
```

`materialize` may assume the existing whitelist just passed; it must read the
same `numbers_` entries and DOM. Map ExactDecimal fields to the new number DTO,
not to a float. Use `boundedInteger` for width/height/endFrame, layer id and
interval, first/last keyframe and the position animation flag. Do not use
DOM integer getters on numerically equivalent non-integer spellings. Copy
names with explicit string length; preserve optional absence. No semantic
validation duplicate, altered order, static state or parse-only cache.

- [ ] **Step 4: Add complete edge assertions and run GREEN.**

Use fixture string mutations in memory (existing replaceOnce/replaceBetween
idiom), do not change fixtures. Add these exact test cases with actual data:

```cpp
// Each mutation must first assert that it found exactly the intended source.
const auto tiny = decodeNativeEllipseInput(replaceOnce(seed, "\"fr\": 60", "\"fr\": 1e-9999"));
const NativeEllipseDecimal expectedTiny{false, "1", {true, "9999"}};
require(tiny.input && tiny.input->frameRate == expectedTiny, "positive underflow retained");
const auto equivalent = decodeNativeEllipseInput(replaceOnce(seed, "\"fr\": 60", "\"fr\": 6000e-2"));
require(equivalent.input && *equivalent.input == *baseline.input, "equivalent decimal descriptor");
const auto badJson = replaceOnce(seed, "\"fr\": 60", "\"fr\": 240.00000000000001");
const auto bad = decodeNativeEllipseInput(badJson);
const auto old = auditNativeEllipseInput(badJson);
require(!bad.input && bad.admission.code == old.code && bad.admission.path == old.path,
        "rejection classification/path preserved");
```

Also assert tiny ellipse size; a valid arbitrarily long negative exponent
string from prior admission tests; negative zero canonicalized; structural
`6100000000000000000000000000e-26` ->61; near-continuity mismatch rejection;
unknown/duplicate/invalid UTF-8/oversized/deep/node-limit rejection with exact
old code/path and no input. Prefer augmenting existing `expectAccepted` /
`expectRejected` helpers to check descriptor presence/parity for the full
existing admission matrix instead of copying that matrix into a second file.

For static position replace the ellipse position object only with
`{"a":0,"k":[-32768,32768]}` and assert the static variant values. Assert
layer interval [10,20), nondefault translation/ID and fill/size values so
hardcoded fixture fields cannot pass. Reorder keys, remove names, use empty
and UTF-8 names; verify exact retained optional values. TGS decode the owned
`tests/compatibility/tgs/telegram_sticker_basic.tgs` through Formats and require
full descriptor equality with its JSON seed.

Retain `shared_ptr<const NativeEllipseInput>` after destroying/overwriting the
source buffer, then compare all fields. Run a bounded pair of threads with
distinct modified names/translations; capture errors to join before asserting,
compare explicit fields and prior retained objects. This tests isolation,
not TSan or general race freedom. Use static_assert for const pointee.

```text
cmake --build --preset windows-msvc-telegram-debug --target avemotion_native_ellipse_input_tests avemotion_native_ellipse_admission_tests --parallel 4
ctest --preset windows-msvc-telegram-debug -R avemotion.runtime.native_ellipse --output-on-failure
git diff --check
```

- [ ] **Step 5: Self-review, scoped commit and independent task review.**

Remove the scaffold, check no public/Runtime caller changes, no conversion,
no new references in none. Document RED/GREEN output, warnings if any, and
files. Commit only the listed product/test changes on main, no push by worker:

```text
git add src/runtime/NativeEllipseInput.hpp src/runtime/NativeEllipseAdmission.cpp tests/native_ellipse_input_tests.cpp tests/native_ellipse_admission_tests.cpp CMakeLists.txt
git commit -m "feat: retain exact owned native ellipse input"
```

Controller dispatches task-scoped spec/quality review with the full base..head
diff and addresses findings through the original worker, not controller edits.

## Final integration gate and handoff (controller)

- [ ] Fresh configure/build/full CTest for `windows-msvc-telegram-debug`,
  `windows-msvc-win32-preview`, `windows-msvc-direct2d`, sequentially. Use
  `cmake --preset NAME`, `cmake --build --preset NAME --parallel 4`,
  `ctest --preset NAME --output-on-failure`. Keep logs/exit codes.
- [ ] `python scripts/verify_vendor.py --variant all` and
  `python scripts/generate_tgs_compatibility_corpus.py --check`.
- [ ] Inspect no-reference build graph for absence of NativeEllipseAdmission,
  NativeEllipseInput, TelegramParsedModelBuilder and rlottie compile/link inputs;
  verify new descriptor/test remains Telegram-only in CMake. No install execution.
- [ ] Whole-stage independent review, at most one consolidated final fix wave
  with covering fresh tests and one scoped re-review.
- [ ] Update durable ledger, STATE and `docs/PART26B_OWNED_NATIVE_INPUT_REPORT.md`
  with exact SHAs/commands/gates, limitations and next correspondence stage.
- [ ] Ordinary scoped docs commit, remote compare before push, then verify exact
  equality/clean tree. Foreign remote stops push only; no rebase/force.

Plan self-review: all spec fields map to Step1/3/4; all five Review Focus
classes have explicit Step4 assertions; no later interface consumes a renamed
field; no product-route, dependency or host changes. One review-worthy product
task (declarations/config/tests belong to the same deliverable). Execution is
SDD, selected by the controller under the user's prior explicit delegation.
