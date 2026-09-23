# AveMotion Part25E — Lean module and optional comparison laboratory

Status: written specification approved by the controller under the user's
explicit planning/approval delegation, 2026-09-23. Execute only after Part25D
correctness, performance evidence and independent handoff; do not interrupt its
already-defined Samsung gates. No Part25E product change is authorized by this
document before its implementation plan and preflight are complete.

## Intent and actual product boundary

The user wants a professional Windows animation component for future AveVoice
integration, with nothing unnecessary in its ordinary build or output. They
approved making Samsung an optional development comparison after the current
persistent-session work. This means separation of build and delivery surfaces,
not deleting source history or claiming a renderer independence that does not
yet exist.

Today Telegram and Samsung are mutually exclusive rlottie choices. Removing the
Samsung source directory would not remove a second engine from a Telegram
binary or establish an FPS improvement. Runtime uses its selected upstream
directly, whereas AveMotion::Reference is a laboratory facade. The no-reference
installed package cannot load/evaluate Lottie animations; its Runtime reports
ReferenceUnavailable. Telegram-backed installation is deliberately prohibited.
Keep these distinctions visible in documentation and verification.

Success here is two honest, lean build surfaces plus an optional Samsung lab:

1. A Telegram-backed **internal module build** with the existing product static
   libraries and Windows Direct2D, without lab/test/tool/preview outputs.
   This remains a build-tree integration surface, not a redistributable package.
2. A **no-reference package** exporting only existing product targets/headers,
   with a verified external consumer and explicit loader limitations.
3. Ordinary CI tests Telegram and no-reference configurations; Samsung is an
   explicit manual comparison workflow. All source, notices, patches, presets
   and full verification commands remain available.

This is not one physical DLL/archive or the final host-facing MotionService.
Do not introduce a misleading umbrella target, stable ABI promise or new public
facade. Existing product targets remain unchanged.

## Alternatives and selected approach

- **Selected: opt-in lean presets and exact package/CI boundaries.** Preserve
  existing development presets, add two lean profiles, suppress an otherwise
  unconditionally built characterizer and exclude the lab facade from default
  build/install output. Small, testable and reversible.
- **Global module/laboratory build-mode rewrite.** Could make a plain configure
  lean, but changes defaults and cache semantics throughout established tooling.
  Unnecessary for the user's immediate integration path; defer.
- **Physically delete Samsung or remove all upstream code now.** Does not solve
  the Telegram Runtime dependency, discards comparison utility, and conflates
  packaging with native evaluation/licensing. Out of scope.

## Binding constraints

- Preserve Runtime/model/rendering behavior, public product API and target names,
  goldens, fallback policy, Direct2D ownership and Player threading.
- Preserve both vendor trees, patches, manifests, source commit identities,
  licenses/notices, committed corpus bytes and all prior raw evidence.
- Keep the reference-linked AVEMOTION_ENABLE_INSTALL fatal guard intact.
- Do not install dependencies, alter global Git/Windows settings, create a new
  publication, change licenses or dispatch external workflows manually.
- Existing developer presets remain usable; clean product presets explicitly
  disable every tool/test/demo switch rather than relying on inherited defaults.
- No performance, race-detector, single-DLL or redistribution claim follows from
  this build-boundary stage. Part25D measurements retain their original scope.

## Lean build surfaces

Add a hidden module-base configure preset inheriting the existing generator and
output-directory convention. Explicitly set BUILD_TESTING=OFF and all laboratory
AVEMOTION_BUILD_* switches OFF, including contract/capture tests and preview.
Direct2D is the sole build switch enabled for Windows product profiles; disable
it explicitly for Linux profiles. Release configuration is explicit.

Public configure/build presets:

- windows-msvc-module-release: Telegram, install OFF, Direct2D ON.
- linux-gcc-module-release: Telegram, install OFF, Direct2D OFF.
- windows-msvc-offline-package: none, install ON, Direct2D ON.
- linux-clang-offline-package: none, install ON, Direct2D OFF.

Keep current Debug/Release/corpus/preview presets unchanged. Add
AVEMOTION_BUILD_SOURCE_GEOMETRY_CHARACTERIZER, default ON for compatibility;
guard both its executable and its two characterization/golden CTests on that
option/target. Core source-geometry unit tests must remain enabled independently
when BUILD_TESTING is ON. A tests-ON/tools-OFF configuration must not register a
command pointing at the absent characterizer.

Declare avemotion_reference STATIC EXCLUDE_FROM_ALL. Retain its build-tree alias,
headers and explicit target; lab consumers pull it in through existing links.
Product libraries must not gain a link to it. A clean lean build must neither
compile ReferenceRuntime.cpp nor produce its archive, lab executables, corpus
analysis archive, preview or tests. The selected Telegram archive is expected
in the internal module build, never Samsung. The offline build has neither.

## Installation boundary

Remove avemotion_reference from installed/exported targets and exclude the
include/avemotion/reference directory from installed public headers. Keep all
other existing product targets and headers, including Validation; it is a
library capability, not the validator command-line tool. Direct2D remains
Windows-conditional. Do not alter upstream installation prohibition.

This intentionally stops exporting the laboratory-only AveMotion::Reference
from the offline package. Document migration for experimental consumers: use
the source/build-tree lab target instead. Existing product CMake target names,
package version requirement and public header layout remain compatible.

Verify a fresh install prefix against a product allowlist: Formats, Core, Model,
Validation, Evaluation, Runtime, Player, Rendering, plus Direct2D on Windows.
Reject Reference exports/headers/archive, executables, lab/test/corpus artifacts,
rlottie/Samsung payloads and source/build-tree path leakage in installed CMake
files. Run the real external find_package consumer against a relocated copy of
the prefix as well as its original path; reject injected extra artifacts in a
standalone regression. Never clean a user prefix or reuse one to hide leftovers.

The consumer should explicitly assert that Reference is not an imported target,
retain existing product linking, and check the documented no-reference loader
error. It must not imply successful animation loading. Separately configure a
Telegram build with ENABLE_INSTALL=ON and require the existing specific fatal
diagnostic; a compiler/configuration failure is not a passing negative test.

## Vendor integrity and optional Samsung CI

Extend scripts/verify_vendor.py with --variant all|telegram|samsung|none,
default all. Selection controls only which vendor trees are fingerprinted;
the committed corpus SHA checks still run for every selection. Invalid values
fail argument parsing. Preserve existing fingerprint ordering/format and exact
bytes. Output must name what was verified, never say both variants for one.

CTest avemotion.vendor.verify uses telegram for Telegram builds, all for Samsung
comparison builds, and none for no-reference builds. Synthetic ordering and Git
protected-byte regressions remain normal development checks. Unit tests use
temporary synthetic trees/manifests to prove selected corruption fails,
unselected missing/corrupt trees do not interfere, all still checks both, and
corpus corruption fails even with none. No actual vendor tree is moved/deleted.

Keep mandatory CI's Telegram Debug/Release/ASan, no-reference, WARP, capture and
preview checks. Remove Samsung entries from its push/pull_request matrices and
use explicit telegram vendor verification there. Add a separate manual
workflow_dispatch-only Samsung comparison workflow retaining existing Linux
Debug/Release/ASan and Windows Debug configurations, full all-vendor verification
and diagnostic artifacts. No continue-on-error, golden exclusion or success
override: existing Samsung golden failures stay visible when that workflow runs.
Do not execute it just to establish that a manual workflow exists.

Add lean internal-build and offline-package checks to normal CI on both OSes,
without replacing existing correctness gates. New Python/CMake checks use only
existing runtimes/standard libraries; no YAML parser/action/dependency install is
required. Review workflow syntax against official documentation at execution.
Linux/cloud checks not available locally must be labeled unverified until actual
CI evidence exists, not inferred from Windows or static inspection.

## Verification and delivery

Use TDD and serialized product tasks: functional boundary test RED before CMake
edits; verifier-selection RED before implementation. Each task gets independent
spec/quality review, fresh controller checks and an ordinary main push. Preserve
the user's delegated approval and existing active automation between tasks.

Gate clean Windows internal module build, no-reference configure/build/install,
prefix manifest, original/relocated consumer, negative reference install,
tests-ON/characterizer-OFF registration, full Telegram and explicit Win32 graphics
suite. Record actual archives/executable absence and compile/link source paths;
do not infer lean output from option values alone. Verify vendor/corpus/license
bytes unchanged and full all-variant integrity manually at stage handoff.

Report the actual delivery boundary and migration, exact commands/SHAs, retained
Samsung baseline limitations and unexecuted Linux/cloud checks. Native topology,
complete fallback decisions, host MotionService and a real runtime redistribution
package remain separately designed follow-on stages; no license decision is made.

## Self-review and delegated approval

The two build surfaces deliberately distinguish working internal Runtime from
an offline installable library set. Existing lab presets stay compatible; one
unguarded characterizer and leaked lab package API have explicit remedies/tests.
Samsung becomes optional without disabling its opt-in verification. No content
deletion or license/install-guard bypass is required. The controller approves
this written specification under the user's explicit autonomous delegation;
implementation plan and preflight follow before any product edit.
