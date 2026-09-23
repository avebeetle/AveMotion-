# Lean Module Boundaries Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Separate a lean internal animation module and an honest offline package from development tools, with Samsung comparison explicitly optional.

**Architecture:** Existing product libraries keep their interfaces and Runtime behavior. Opt-in lean presets exclude lab targets; package exports contain product APIs only. Variant-scoped integrity checks let ordinary Telegram/no-reference CI run independently of the retained manual Samsung laboratory.

**Tech Stack:** Existing CMake3.25+/Ninja/MSVC, C++20, Python standard library, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-23-lean-module-boundaries-design.md`

## Global Constraints

- Preserve Runtime/model/rendering behavior, public product API and target names, goldens, fallback policy, Direct2D ownership and Player threading.
- Preserve both vendor trees, patches, manifests, source commit identities, licenses/notices, committed corpus bytes and all prior raw evidence.
- Keep the reference-linked AVEMOTION_ENABLE_INSTALL fatal guard intact.
- Do not install dependencies, alter global Git/Windows settings, create a new publication, change licenses or dispatch external workflows manually.
- Existing developer presets remain usable; clean product presets explicitly disable every tool/test/demo switch rather than relying on inherited defaults.
- No performance, race-detector, single-DLL or redistribution claim follows from this build-boundary stage. Part25D measurements retain their original scope.

Start only after Part25D handoff. User chose main and delegated written approval
and execution decisions. Controller selects SDD, one product implementer at a
time, fresh task review, small scoped commits and ordinary verified pushes.
Workers never push or stage controller STATE/plan/spec. Existing tool versions,
dependency versions and package version stay unchanged. Existing development
presets and default verify_vendor.py all-variant behavior remain compatible.

## Review Focus

1. A cached tool option remains ON through preset inheritance: explicit lean values and actual built-output checks, not option inspection alone (Task1).
2. BUILD_TESTING=ON with source characterizer OFF registers a missing executable: guard only tool-driven CTests, retain source-geometry unit test (Task1).
3. A package appears lean but leaks lab exports or source-tree paths: fresh and relocated prefix checks plus real consumer (Task2).
4. Scoped vendor verification silently skips the selected tree or corpus: corrupt/missing synthetic inputs and none-mode corpus failure (Task3).
5. Optional Samsung accidentally remains mandatory or is made falsely green: workflow trigger/matrix audit, preserve unrelaxed tests and explicit full verification (Task3/4).

## Files and interfaces

- `cmake/AveMotionOptions.cmake`, root `CMakeLists.txt`: one characterizer flag,
  Reference default-build/install boundary, variant-selected CTest integrity.
- `CMakePresets.json`: hidden lean preset and four public configure/build pairs.
- `scripts/test_module_build_boundaries.py`: verifies already-built lean output;
  no implicit build, deletion or configuration changes.
- `scripts/test_installed_package_boundaries.py`: verifies fresh installed prefix
  and rejects seeded boundary violations with its standard-library self-test.
- `tests/consumer/CMakeLists.txt`, `tests/consumer/main.cpp`: product import guard
  and honest no-reference Runtime behavior.
- `scripts/verify_vendor.py`, `scripts/test_vendor_selection.py`: explicit vendor
  selection without fingerprint changes, synthetic mutation tests.
- `.github/workflows/ci.yml`, `.github/workflows/samsung-comparison.yml`: primary
  correctness/package lanes and manual unrelaxed comparison respectively.
- `README.md`, new `docs/MODULE_BUILD.md`, final `docs/PART25E_LEAN_MODULE_REPORT.md`:
  commands, boundary/migration/limitations and concrete evidence.

### Task 1: Lean internal and offline build profiles

**Files:** options, root CMake, presets, new module-boundary script,
`docs/MODULE_BUILD.md`, README primary build pointer.

**Interfaces:** Add option AVEMOTION_BUILD_SOURCE_GEOMETRY_CHARACTERIZER defaultON;
public configure/build presets windows-msvc-module-release,
linux-gcc-module-release, windows-msvc-offline-package,
linux-clang-offline-package. Boundary script CLI:
`--build-dir PATH --variant telegram|none --direct2d yes|no`.

- [ ] Read the spec and actual CMake tool/test guards, including source geometry
  characterization near the source-geometry unit target. Capture dispatch BASE
  and protected vendor/license/corpus hashes. Use fresh unique build directories
  under out/part25e; do not erase old build output to make an absence check pass.
- [ ] Write the post-build boundary verifier first. Read CMakeCache.txt to require
  Release, expected variant/Direct2D, BUILD_TESTING=OFF and every tool switchOFF.
  Require existing product archives; reject Reference/corpus_analysis archives,
  lab/test/preview executables and built ReferenceRuntime/app/test objects.
  Require selected Telegram archive only for telegram; none has no upstream
  archive. A rejected cache or absent product artifact must exitnonzero.

```python
required = {"avemotion_formats", "avemotion_core", "avemotion_model",
            "avemotion_validation", "avemotion_evaluation", "avemotion_runtime",
            "avemotion_player", "avemotion_rendering"}
if direct2d:
    required.add("avemotion_backend_d2d")
# Discover platform .lib or lib*.a basenames, compare with required;
# reject avemotion_reference/avemotion_corpus_analysis and unexpected executables.
```

- [ ] Configure a fresh old-code Telegram Release build with all CURRENT tool
  flagsOFF, testingOFF and installOFF; build the default target. Run the new
  verifier and retain functional RED for the unguarded source characterizer
  and/or built Reference archive, not a missing compiler/new-preset error.
  Supplement verifier unit self-tests using temporary artifact/cache copies to
  reject one forbidden output, missing product archive and stale ON option.
- [ ] Add the defaultON source-characterizer option; guard its executable and
  only its characterization/golden registration. Leave source geometry unit
  test unchanged. Mark the Reference library EXCLUDE_FROM_ALL:

```cmake
add_library(avemotion_reference STATIC EXCLUDE_FROM_ALL
    src/reference/ReferenceRuntime.cpp
    src/reference/UpstreamInfo.cpp)
if(AVEMOTION_BUILD_SOURCE_GEOMETRY_CHARACTERIZER
   AND AVEMOTION_RLOTTIE_AVAILABLE
   AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    # Existing source-characterizer definition stays here unchanged.
endif()
# Inside the existing Telegram testing scope:
if(TARGET avemotion_source_geometry_characterize)
    # Existing characterize + source_geometry_golden block only.
endif()
```

Retain every existing Reference source/definition/include/link property; the
snippet names the changed declaration, not permission to remove extra sources.

- [ ] Add hidden module-base inheriting base but explicitly setting testingOFF,
  Release, every existing tool/test/demo switchOFF plus the new source flagOFF;
  disable Direct2D there and enable only in Windows leaf profiles. Use current
  host conditions/compiler conventions. Telegram leaves installOFF; none setsON.
  Add build presets, no misleading empty test presets. Re-read the actual option
  inventory to ensure a newly discovered flag is also explicit.
- [ ] GREEN default builds and actual-output verifier for both Windows leaves.
  Configure a third Telegram testsON/sourcecharacterizerOFF build and inspect
  `ctest --show-only=json-v1`: no source_geometry_characterize or golden commands,
  but avemotion.render.source_geometry remains. Build/run that unit test. Prove
  existing default developer preset still registers both characterization tests.
- [ ] Document internal build versus offline package limitations, ordinary
  command examples and no single-DLL/license promise. Run focused legacy
  subproject and full Telegram checks affected by CMake. Commit only scoped
  files; report RED/GREEN/commands/artifact lists; independent review then push.

### Task 2: Product-only installed package and honest external consumer

**Files:** root CMake installation block, new installed-boundary script,
tests/consumer CMake/main, MODULE_BUILD guide.

**Interfaces:** Script CLI `--prefix PATH --direct2d yes|no`, plus `--self-test`
for synthetic mutation coverage. Existing find_package(AveMotion0.24.0 CONFIG)
and product targets retain names. Task1 offline presets supply lean build.

- [ ] Write boundary verifier and consumer guard before install changes. Require
  product import declarations and expected header/library layout, no Reference,
  lab/test/CLI/preview/upstream files, and no absolute source/build path leakage
  in installed CMake. Read export target declarations, not substring search of
  the whole source tree. Test real fresh prefix then independently mutated
  temporary copies with unwanted reference header/export/archive, executable,
  leaked path and missing product file.

```cmake
find_package(AveMotion 0.24.0 REQUIRED CONFIG)
if(TARGET AveMotion::Reference)
    message(FATAL_ERROR "Installed product package exposes laboratory Reference")
endif()
```

- [ ] Install existing offline code into a unique prefix and retain functional
  RED identifying its Reference exports/header/archive. This is a packaging
  boundary failure, not a loader or compiler failure.
- [ ] Remove Reference from install(TARGETS); exclude only its public-header
  subtree, preserving all product headers and Windows Direct2D conditional.

```cmake
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN "avemotion/reference" EXCLUDE)
```

Verify the actual install result: if this pattern is unsuitable for CMake's
directory matching, use explicit product directory installs with identical
layout; do not assume the pattern worked from configure success.

- [ ] Extend the real consumer with documented offline Runtime loading rejection
  using the public Runtime/load API and RuntimeErrorCode::ReferenceUnavailable.
  Supply a small valid JSON input; malformed-input rejection is not proof of
  missing reference capability:

```cpp
avemotion::runtime::Runtime runtime;
const auto loaded = runtime.loadLottieJson(
    R"({"v":"5.7.4","fr":30,"ip":0,"op":2,"w":16,"h":16,"layers":[]})",
    "offline-consumer");
if (loaded || loaded.error.code !=
    avemotion::runtime::RuntimeErrorCode::ReferenceUnavailable) {
    return 12;
}
```

  Keep its product-link/type/evaluator/rendering checks unchanged.
- [ ] Configure/build/install Task1 Windows offline preset, validate fresh prefix,
  configure/build/run consumer there. Copy prefix to a new temporary location
  without deleting originals; repeat consumer configure/build/run using only
  relocated CMAKE_PREFIX_PATH. Both must pass with no source-tree linkage.
- [ ] In a separate fresh configure request Telegram+ENABLE_INSTALLON and assert
  nonzero plus exact existing installation-prohibition diagnostic. Verify the
  trace reached that guard, not compiler detection failure. Keep guard untouched.
- [ ] Document Reference migration to source/build-tree lab target and offline
  load limitation. Run none/Direct2D full suite and consumer/prefix tests, retain
  lists/raw logs, commit scoped changes, independent review and ordinary push.

### Task 3: Scoped integrity and explicit optional Samsung CI

**Files:** verify_vendor.py, new test_vendor_selection.py, CMake CTest wiring,
existing ci.yml, new samsung-comparison.yml, MODULE_BUILD/README guidance.

**Interfaces:** `verify_vendor.py --variant all|telegram|samsung|none` defaultall;
selection narrows vendor fingerprints only, never committed corpus SHA checks.
Extract a testable `verify(root: Path, variant: str) -> list[str]` using a manifest
relative to suppliedroot; main uses existingROOT and prints selected scope.

- [ ] Write synthetic verification tests first, constructing temporary minimal
  vendor trees/SOURCE_COMMIT/manifest and corpus/sums with real tree_fingerprint.
  Require all/default success, selected corruption/missingtree failure,
  telegram success when only Samsung is corrupt/missing, none success without
  either tree, corpus corruption failure for allfour selections and invalid
  CLI selection rejection. Preserve actual vendor/source files untouched.
- [ ] Capture functional RED via CLI `--variant telegram` against synthetic
  selection harness (or legacy verifier called on syntheticroot): old behavior
  checks unselected Samsung and fails. An unrecognized option alone is not the
  substantive RED; include the actual unwanted tree requirement.
- [ ] Implement argparse selection and extracted verifier without changing
  canonical_tree_files/fingerprint_entries format or existing checks:

```python
selected = data["variants"] if variant == "all" else (
    {} if variant == "none" else {variant: data["variants"][variant]})
for name, entry in selected.items():
    source = root / entry["source_directory"]
    # Move the existing checks of CMakeLists/COPYING/SOURCE_COMMIT, exclusions,
    # tree hash, file count and byte count here without algorithm changes.
# Existing corpus hash checks run after that loop in every mode.
```

CLI choices are exact and unknown names fail before verification.

- [ ] CTest selection: Telegram->telegram, Samsung->all, none->none; retain
  avemotion.vendor.verify name plus ordering/protected-byte tests. Register
  self-contained selection regression with existing Python interpreter.
  GREEN synthetic tests, actual --variantall and Telegram/none CTest commands.
- [ ] Check official GitHub Actions docs for workflow_dispatch and matrix syntax.
  Move existing Samsung LinuxDebug/GCCRelease/ASan+WindowsDebug lanes to a new
  workflow_dispatch-only workflow. Preserve full checks, compiler setup and
  diagnostics, no continue-on-error/testexclusion. Mandatory ci retains every
  Telegram/no-ref/graphics lane; direct verifier calls there use telegram.
- [ ] Add ordinary Linux/Windows lean internal-build and offline-package CI
  checks using Task1/2 scripts and existing compiler/Python setup. Existing
  correctness/offline tests remain. Run original/relocated consumer and guarded
  negative install in these lanes. Avoid third-party new actions/dependencies
  just to parse YAML; use existing installed parser if available, otherwise
  validate structure through source inspection and documented CI limits.
- [ ] Test actual CTest generated argv for allthree configurations; audit CI
  no Samsung preset/dual-tree requirement on automatic lanes and no automatic
  trigger on manual workflow. Run synthetic verifier tests, existing fingerprint
  ordering/protected bytes and full Telegram. Commit/review/push; do not manually
  dispatch external comparison workflow or conceal its known golden failures.

### Task 4: Whole-boundary verification and handoff

**Files:** final report, controller STATE/ledger. No additional feature changes.

**Interfaces:** consumes reviewed commits; reports actual lean output, package
and optional-lab state, not Runtime independence or performance improvement.

- [ ] Run fresh Windows module and offline build/prefix/consumer/relocation gates,
  reference-install negative check, testsON/sourcecharacterizerOFF target gate,
  full Telegram, explicit windows-msvc-win32-preview and none/Direct2D suites.
  Preserve exact shell commands, native exits and capture/WARP/device recreation.
- [ ] Verify all-variant fingerprints and unchanged vendor/license/corpus/golden
  bytes. Record Samsung sources/presets/manual workflow retained; do not call it
  allgreen or alter the two known Polystar failures. Linux/cloud status is only
  what real evidence establishes; if not run, mark unverified.
- [ ] Independently review whole stage against saved stageBASE, package evidence
  and deferred-minor ledger. One bounded fix wave/scoped review if necessary,
  then fresh covering checks and ordinary push. No forcepush/deletion.
- [ ] Write handoff with SHAs, file/target manifests, measured build-output
  separation, migration and remaining limitations. Keep automation active and
  proceed to separately designed native-topology/MotionService work, not an
  inferred licensing/publication step.

## Self-review and delegated execution approval

All spec sections have owning tasks: lean outputs1, installation2, optional
comparison3, full evidence4. Shared CMake/docs are serialized; no Runtime or
vendor product source is edited. Review-focus failures each have explicit
negative checks. The controller approves this written plan and SDD using the
user's existing delegation; start remains gated on Part25D handoff.
