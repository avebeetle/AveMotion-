# Native Ellipse Binding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Certify exact admitted input against authored model and one observable full-timeline render slot, without activating native playback.

**Architecture:** Pure authored binder followed by a private cold factory using public Runtime APIs. A separate streaming audit validates every scene and retains only immutable slot metadata plus ownership leases. Existing callers are unchanged.

**Tech Stack:** C++20, MSVC, CMake/CTest, current Telegram reference and standard from_chars; installed Python for provenance checks.

**Spec:** docs/superpowers/specs/2026-09-24-native-ellipse-binding-design.md

## Global Constraints

- Work directly in main; ordinary scoped commits/push only, no history rewriting.
- Preserve Part25I admission codes, paths, grammar and resource limits.
- Keep new code private and Telegram-only; none/Samsung must not acquire it.
- No production caller selects the new factory or native route in this stage.
- Do not change Runtime.cpp, public APIs/fallback, Player threading, Direct2D ownership or ANGLE/backend coverage.
- Do not modify vendor sources, versions, patches, licenses/notices, fixtures or goldens.
- Do not install dependencies, change Windows settings, or write/build/run GUI in Avelabs-UI.
- Preserve raw evidence and SDD records; do not resume or create an automation.

## Review Focus

1. Underflow and fractional frame rates must not confuse raw admission with pinned float interpretation (Task1 numeric matrix).
2. Semantically identical reordered tables must bind while wrong backreferences fail (Task1 remapping and mutation matrix).
3. A union model must not conceal missing/duplicate draws or inactive-layer mistakes (Task2 streaming mutations).
4. Hash equality must not authorize a certificate for a different asset lease (Task2 same-byte different-load rejection).
5. Late scan failures must not publish partial/stale certificates or retain mutable evaluators (Task2 poison/lifetime/isolation tests).

---

### Task 1: Exact numeric interpretation and authored model binder

**Files:**
- Create src/runtime/NativeEllipseBinding.hpp and .cpp.
- Create tests/native_ellipse_binding_tests.cpp.
- Modify CMakeLists.txt only Telegram private sources/test registration.
- Raw logs out/part26c/task1/; full report task-1-report.md in this plan's SDD workspace.

**Interfaces:**
- Consumes NativeEllipseInput from NativeEllipseInput.hpp and MotionAssetModel.
- Produces NativeEllipseBindingCode, NativeEllipseModelBinding, NativeEllipseBindingResult and bindNativeEllipseModel exactly as the spec Task1 contract.
- Task2 consumes the returned IDs by name, not table constants. Read spec numeric and Task1 sections fully. No source ownership claims from this pure binder.

- [ ] **Step 1: Add contract, failing test and Telegram-only target.**

Start with a functional stub returning the default empty binding. Test real
baseline through decodeNativeEllipseInput and Runtime load/prepareModel:

```cpp
auto input = decodeNativeEllipseInput(json);
runtime::Runtime runtime;
auto asset = runtime.loadLottieJson(json);
auto model = asset.asset->prepareModel();
require(input && asset && model, "baseline prerequisites");
auto result = bindNativeEllipseModel(*input.input, *model.model);
require(static_cast<bool>(result), "baseline authored binding published");
require(result.binding->position.valid(), "position resolved");
```

Register `avemotion_native_ellipse_binding_tests` and CTest
`avemotion.runtime.native_ellipse_binding` exactly beside existing input tests;
link AveMotion::Runtime, include src/runtime, fixture-dir definition, warnings.
No changes to prior fixtures/tests are necessary. Run configure/build with
windows-msvc-telegram-debug and `ctest --preset windows-msvc-telegram-debug -R
native_ellipse_binding --output-on-failure`; retain the functional failure.

- [ ] **Step 2: Implement bounded numeric conversion and checked topology.**

Use compact canonical token and explicit range guards:

```cpp
const auto token = sign + decimal.digits + "e" + powerSign + decimal.power.magnitude;
double parsed = 0;
auto converted = std::from_chars(token.data(), token.data()+token.size(), parsed);
// Require complete conversion, finite, nonzero preservation, float range.
// All float fields including frame rate use checked double -> float.
```

Check canonical private decimal format rather than accepting forged empty
digits/sign/exponent. Resolve graph using model lookup plus checked ranges;
check exactly the spec source/default/property/track/value invariants. Keep
helpers local and focused by numeric, table, topology, property responsibilities.
Never read out-of-range data even on malformed synthetic models. Return no
binding on any failure. No generic graph framework or reference frame sampling.

- [ ] **Step 3: Add behavioral matrix with actual mutated model/input values.**

```cpp
auto changed = *model.model;
changed.properties[result.binding->position.index()].owner = result.binding->fill;
require(!bindNativeEllipseModel(*input.input, changed), "wrong owner rejected");
changed = *model.model;
changed.sourceNodes.push_back(changed.sourceNodes.back());
require(!bindNativeEllipseModel(*input.input, changed), "extra authored node rejected");
```

Use in-memory JSON replacements, preserving committed fixtures: static position;
activity[10,20); empty/missing/UTF-8 names; equivalent decimals; fractional fr59.94;
linear controls and cubic controls; nonzero float subnormal that matches model;
accepted raw underflow1e-9999 and overflow fail numeric eligibility. Mutate owner,
semantic duplicate, index/range overflow, parent/composition, name/hash, flags,
dependency bits, extra node/property/track, value type/value, segment controls/
times/tangents and confirm empty failure. Build a correctly reindexed source and
property-table clone with all edge/owner/backreferences adjusted; it must bind.
Assert effective fallback names and exact fr==double(float(parsed59.94)).

- [ ] **Step 4: GREEN, full suite, self-review, scoped commit and report.**

Run focused binding/admission/input tests until green, then one full Telegram
CTest on current build. `git diff --check`; inspect own diff. Commit only listed
product/tests/CMake files as `feat: bind owned ellipse input to authored model`.
Do not push; controller reviews. Report commands/raw paths, functional RED,
GREEN/full summary, limitations, signatures and commit. No worker subagents.

### Task 2: Cold certificate factory and streaming slot audit

**Files:**
- Create src/runtime/NativeEllipseCertificate.hpp/.cpp and NativeEllipseScanAudit.cpp.
- Create tests/native_ellipse_certificate_tests.cpp.
- Modify CMakeLists.txt only Telegram private sources/test registration.
- Logs out/part26c/task2/ and task-2-report.md in this plan's SDD workspace.

**Interfaces:**
- Consumes Task1 `NativeEllipseBindingResult bindNativeEllipseModel(const NativeEllipseInput&, const model::MotionAssetModel&)`; binding has root/layer/group/ellipse/fill and eight property IDs as spec.
- Produces `prepareNativeEllipseCertificate(std::string_view)` returning private status plus const shared certificate, and `NativeEllipseScanAudit` with constructor taking input/binding/frozen model/expected handle/hash, observe(frame,scene), finish().
- Define focused metadata/result types in the private header: explicit scan error categories in spec, fixed layer/draw value metadata, lease checks and diagnostic snapshot. No public header or Runtime.cpp edit.

- [ ] **Step 1: Declare private contract and functional RED.**

Declare success-only certificate factory stub, then exercise real baseline:

```cpp
auto prepared = prepareNativeEllipseCertificate(json);
require(static_cast<bool>(prepared), "complete baseline certificate published");
require(prepared.certificate != nullptr, "certificate ownership");
```

Register `avemotion_native_ellipse_certificate_tests` /
`avemotion.runtime.native_ellipse_certificate`, same private include/link/fixture
pattern as binding target. Capture failing focused CTest after successful build.

- [ ] **Step 2: Implement factory ownership and streaming audit.**

Factory control flow:

```cpp
// Own exact JSON; decode; load SAME bytes; prepare; bind; create separate Instance.
// Check model/asset metadata identity before constructing audit.
for (std::size_t frame = 0; frame < input->endFrame; ++frame) {
    auto evaluated = instance.instance->evaluateFrame(frame, input->width, input->height);
    // Evaluation error -> empty result; otherwise audit.observe(frame, evaluated.scene).
}
// finish -> immutable metadata; record runtime.diagnostics(); publish external bundle.
```

Adapt InstanceResult member spelling to actual Runtime header, not a new wrapper.
Validate every invariant in spec Task2. Keep paths and mutable scenes local to
the loop; store only fixed slot metadata and leases. `matchesAsset` or equivalent
must require pointer identity plus handle/hash/model identity, not hash alone.
Failed audit is permanently poisoned and cannot return a partial result.
Expose diagnostic snapshots without adding global counters. Check N model and
N scene samples separately; no ordinary caller or install boundary changes.

- [ ] **Step 3: Add full-timeline and fail-closed behavioral tests.**

```cpp
// Obtain normal real scenes from a separately loaded baseline Asset prepared first.
// Construct an audit with its actual input/binding/model/handle/hash.
auto scene = instance.instance->evaluateFrame(activeFrame, width, height).scene;
scene.drawItems.push_back(scene.drawItems.front());
require(!audit.observe(activeFrame, scene), "duplicate draw rejected");
require(!audit.finish(), "failed audit cannot publish");
```

Feed strict ordered scans (not isolated frame10 as first observation) before a
target mutation. Cover baseline, static, activity[10,20), all boundary frames,
early finish/skips/repeats, duplicate/extra/missingdraw, wrong IDs/local flags,
byte drift, class demotion/final model disagreement, identity/counts/layout.
Keep certificate after factory internals die; load same bytes anew and reject
lease matching. Concurrent two independent factories must not share mutable
state. On ineligible raw/model/scan cases, independently ordinary load/evaluate
still succeeds when valid for the reference. Assert expected counters and all
stored field invariants, not only certificate non-null.

- [ ] **Step 4: GREEN, full suite, self-review, scoped commit and report.**

Run focused certificate/binding/input tests, one full Telegram suite and diff
check. Commit only listed files as `feat: certify native ellipse render slots`.
Controller owns review/push/full cross-preset gates. Report exact cold scan cost,
provenance limitation, API field names, raw commands/results and commit.

## Controller closure and next stage

- [ ] Independent task review after each task, record/fix findings, update STATE/ledger and ordinary push only after review and successful checks.
- [ ] Fresh full MSVC Telegram, windows-msvc-win32-preview and none/Direct2D; verify_vendor.py --variant all; generate_tgs_compatibility_corpus.py --check; inspect generated none/Samsung source/link boundaries.
- [ ] Whole-stage independent review over d212a49..HEAD; scoped fix cycle as required, report actual counts/cost/limits and commit/push evidence.
- [ ] Continue separate Part26D design for full native emitter parity; do not report C as complete playback or stop the long request at this checkpoint.

## Plan self-review

Spec numeric/authored requirements -> Task1; ownership/full scan and failures ->
Task2; review/provenance/platform -> controller closure. No uncovered requirement
identified. Task1 result names match Task2 consumption. All five review-focus
classes have named tests. No fixture, vendor, Runtime.cpp or UI write is planned.
