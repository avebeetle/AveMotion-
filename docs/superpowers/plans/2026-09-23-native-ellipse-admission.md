# Strict Native Ellipse Admission Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reject any raw input outside the specified one-ellipse grammar before a future native scene compiler can mistake omitted upstream content for support.

**Architecture:** A pure private audit in the Telegram Runtime archive reads bounded JSON using its existing vendored reader. It exposes no public API and is not called by production loading/evaluation in this stage. Tests exercise the actual audit; later native adoption needs a separate design.

**Tech Stack:** C++20, existing Telegram RapidJSON, CMake/CTest, installed MSVC; no new dependencies.

**Spec:** docs/superpowers/specs/2026-09-23-native-ellipse-admission-design.md

## Global Constraints

- No public API, playback routing, model publication, CPU path, Direct2D ownership, Player threading, ANGLE/backend coverage, fallback policy, vendor/golden change, dependency installation, Windows setting or licensing change. No new benchmark.
- Compile the gate only for the Telegram Runtime variant. Its private header has only standard C++ types.
- Every object has an exhaustive allowed-key set; all listed keys are required unless marked optional. Keys may appear in any order. Duplicate keys anywhere, unknown keys anywhere and extra children are rejected, even if upstream ignores them.
- Maximum input: 1,048,576 bytes; empty input rejects. Embedded literal NUL bytes reject before parse, including a valid JSON prefix followed by NUL/trailing data.
- Parse the complete bounded buffer with UTF-8 validation and iterative parsing using existing RapidJSON flags.
- Before semantic recursion, walk the DOM iteratively and reject more than 4096 values, container nesting deeper than 32, or duplicate member names.
- Accepted has an empty path. Rejected has a nonempty JSON-pointer-style location beginning with `/`; use `/` for document-wide failures and escape `~`/`/` in keys.
- Work directly in main under explicit user authority; commit only scoped changes, ordinary push after tests/review. Preserve raw evidence and unrelated changes. Do not recreate the deleted automation.
- Keep at most one product implementer. Do not execute the locally policy-blocked preview PowerShell runner or bypass Windows signature policy; use direct VS-environment CMake/CTest and normal GitHub checkout.

## Review Focus

1. A valid prefix followed by literal NUL/garbage must not be accepted as a complete document — Task1 malformed-input tests.
2. Duplicate keys or ignored operators at any nesting level must not yield false eligibility — Task1 duplicate/unknown/extra-child matrix.
3. A JSON number written `1.0` is numerically integral, while true/string/null are not numbers — Task1 representation/type/boundary cases.
4. Deep/large input must reject without unbounded recursive parse or dangling diagnostics — Task1 iterative parse, resource and returned-result lifetime tests.
5. Samsung/none and installed interfaces must not gain a parser dependency or public admission API — Task2 no-reference/full builds, public-header diff and source-list inspection.

## File map

- Create src/runtime/NativeEllipseAdmission.hpp: private result enum/value and pure audit declaration specified verbatim in the spec.
- Create src/runtime/NativeEllipseAdmission.cpp: length/encoding/syntax/resource checks, explicit grammar visitors, owned diagnostic path.
- Create tests/native_ellipse_admission_tests.cpp: real baseline and in-memory positive/negative mutations; no committed corpus changes.
- Modify CMakeLists.txt: append source only in existing Telegram Runtime branch; add Telegram-only test executable and CTest registration.
- Controller owns STATE, this plan/spec and the final evidence report. No other production file needs changes.

### Task 1: Implement and adversarially test the private admission contract

**Files:** the four implementation/test/build files in the file map.

**Interfaces:** consumes `std::string_view` containing raw UTF-8 JSON, no Asset or model. Produces `avemotion::runtime::detail::NativeEllipseAdmission auditNativeEllipseInput(std::string_view)` and `accepted()` with exact enum/path semantics from the spec. No retained input pointers. This is input grammar acceptance only, not a native-ready certificate.

- [ ] Read the spec's entire grammar table and parser/result rules. Write the C++ test first following existing tests' `require`/exception-main pattern. Read the existing fixture as binary using AVEMOTION_FIXTURE_DIR. A helper replacing an exact fragment must fail if the fragment is absent, preventing inert mutations.

```cpp
const auto baseline = readFixture("telegram_sticker_basic.json");
const auto accepted = auditNativeEllipseInput(baseline);
require(accepted.accepted(), "baseline should satisfy the specified input grammar");
require(accepted.path.empty(), "accepted result has no error path");
auto withUnknown = replaceOnce(baseline, "\"ty\": \"el\"",
    "\"unhandled\": 1, \"ty\": \"el\"");
const auto rejected = auditNativeEllipseInput(withUnknown);
require(!rejected.accepted(), "unknown ellipse field must reject");
require(rejected.code == NativeEllipseAdmissionCode::UnsupportedField,
    "unknown field classification");
require(!rejected.path.empty() && rejected.path.front() == '/', "owned error path");
```

- [ ] Add minimal build wiring/private declarations and a compilable reject-all seam only after tests exist. Run the focused test and retain functional RED at the positive baseline assertion, rather than claiming a missing-symbol error as behavioral proof.

```cpp
NativeEllipseAdmission auditNativeEllipseInput(std::string_view) {
    return {NativeEllipseAdmissionCode::UnsupportedStructure, "/"};
}
```

CTest name: `avemotion.runtime.native_ellipse_admission`. Executable: `avemotion_native_ellipse_admission_tests`. Link only `AveMotion::Runtime`; private include `src/runtime`, fixture macro like neighboring tests. Add this executable only under Telegram and AVEMOTION_BUILD_TESTS. Do not create a new library target or export.

- [ ] Implement the pure audit. Include the pinned `rapidjson/document.h` only from the `.cpp`, via Runtime's existing private Telegram include path. Reject literal NUL/byte limit before parse; parse with `kParseIterativeFlag | kParseValidateEncodingFlag`, never permissive flags. Use the explicit input length.

```cpp
rapidjson::Document document;
document.Parse<rapidjson::kParseIterativeFlag |
               rapidjson::kParseValidateEncodingFlag>(json.data(), json.size());
if (document.HasParseError())
    return {NativeEllipseAdmissionCode::InvalidJson, "/"};
```

Build reusable small helpers for exact allowed/required keys, numeric finite/range/integrality checks, fixed scalar/vector properties, and JSON-pointer path construction. A bounded iterative DOM pass handles all duplicate/resource checks before grammar traversal. Key comparisons must use lengths, not C strings alone. Map failures exactly as the spec requires; do not catch every error and claim Accepted. Do not introduce a second parser or regex admission scanner.

- [ ] Add meaningful accepted cases: baseline; short layer range [10,20); static ellipse position; different valid size/color/layer translation/names; reordered object keys; omitted optional names/version; escaped inert names; integral decimal numbers at integer-required fields; leading/trailing JSON whitespace. Every case must check Accepted plus empty path.
- [ ] Add rejection families, each checking not accepted plus nonempty path and representative exact error codes. Cover every object row in the spec with an unknown key; every required-key family with a missing key; all scalar/vector arities/types; boundary failures for root dimensions/rate/op, range/ip/op, ellipse size/position, RGB/alpha, easing/time continuity; changed fixed transforms/opacity/blend/direction; extra layers/groups/paths/fills; unknown/mm/hidden operators; expressions, hold and spatial tangents; animated size/paint/transform; extra keyframes, array easing and mismatched final value.
- [ ] Add whole-document adversarial cases: empty/whitespace/truncated JSON, trailing object/comment/comma, literal NUL after valid prefix, invalid UTF-8, decoded NUL in a name, duplicate root and nested keys including escaped aliases, boolean/string/null numeric substitutions, NaN/Infinity/overflow numeric tokens, byte-limit +1, 33-container nesting and >4096 values. Verify valid bounded maximum values where the grammar permits them. Check diagnostics remain readable after original input destruction and repeated audits are independent.

```cpp
auto nulSuffix = baseline;
nulSuffix.push_back('\0');
nulSuffix += "ignored";
require(auditNativeEllipseInput(nulSuffix).code ==
        NativeEllipseAdmissionCode::InvalidJson, "NUL cannot truncate input");
auto large = std::string(1'048'577, ' ');
require(auditNativeEllipseInput(large).code ==
        NativeEllipseAdmissionCode::ResourceLimit, "byte limit enforced first");
```

- [ ] Run focused RED/GREEN checks, then all existing Telegram runtime/validation tests. Use installed VsDevCmd environment, not an execution-policy bypass. Record command, native exit, assertion and logs. Inspect protected source/golden/vendor diff and `git diff --check`; self-review for false acceptance and ownership. Commit only the four scoped files. Do not push from implementer. Write complete report in the plan's SDD directory and hand off for independent task review.

Commands under installed VS environment:

```text
cmake --preset windows-msvc-telegram-debug
cmake --build --preset windows-msvc-telegram-debug --parallel 4
ctest --preset windows-msvc-telegram-debug -R avemotion.runtime.native_ellipse_admission --output-on-failure
ctest --preset windows-msvc-telegram-debug -R "avemotion\.(runtime|validation)\." --output-on-failure
```

### Task 2: Verify variant boundaries and close the stage

**Files:** controller-owned STATE and docs/PART25I_NATIVE_ELLIPSE_ADMISSION_REPORT.md; no product changes unless a reviewed Task1 fix is needed.

**Interfaces:** consumes accepted Task1 commit and report. Produces gate logs, coverage/limitations report and reviewed ordinary main push. Review the unchanged-runtime-routing claim through diff and call-site search, not a new behavior flag.

- [ ] Independently review Task1 complete diff against exact grammar, including security/resource input cases and CMake variant guards. Findings return to the original implementer; no controller production fix and no weakening the grammar/tests.
- [ ] Fresh configure/build and full CTest for windows-msvc-telegram-debug, windows-msvc-win32-preview and windows-msvc-direct2d (the existing none/Debug preset in CMakePresets.json). Confirm explicit preview includes WARP/capture/device recreation. Samsung product/source remains untouched; only all-vendor integrity required here, optional comparison need not be reimplemented.

```text
cmake --preset windows-msvc-telegram-debug
cmake --build --preset windows-msvc-telegram-debug --parallel 4
ctest --preset windows-msvc-telegram-debug --output-on-failure
cmake --preset windows-msvc-win32-preview
cmake --build --preset windows-msvc-win32-preview --parallel 4
ctest --preset windows-msvc-win32-preview --output-on-failure
cmake --preset windows-msvc-direct2d
cmake --build --preset windows-msvc-direct2d --parallel 4
ctest --preset windows-msvc-direct2d --output-on-failure
python scripts/verify_vendor.py --variant all
python scripts/generate_tgs_compatibility_corpus.py --check
git diff --check
```

- [ ] Verify no installed/public header changed; no native admission object source is appended for none/Samsung; no production runtime caller exists. Preserve all raw evidence and independent review reports. Report supported grammar, input-ineligibility semantics, no native emitter/playback adoption, no speed claim and next raw-to-model/scene parity stage.
- [ ] Independent whole-stage review, scoped corrections if needed, documentation commit and ordinary push only if remote has no foreign advance. Inspect actual CI result; classify known Linux reference failures separately rather than suppressing them. Do not recreate automation.

## Controller approval and preflight

The user delegated written plan approval and execution selection. Controller
self-review covered all spec grammar rows with Task1 mutation families, parser
limits and result ownership with explicit adversarial tests, and CMake/private
dependency boundaries with Task2. No missing interface or placeholder remains.
The no-reference preset was resolved to windows-msvc-direct2d from the actual
presets file. Spec self-review clarified static ellipse position bounds and safe
default rejection in the result value. Controller approves this written plan and
selects subagent-driven implementation, one product writer, preserved evidence.
