# Part26E — first-party bounded JSON reader

Status: accepted after independent whole-stage/scoped review and fresh final gates
at product/test revision9212c03c1b4e60f8221932c19754964c7bd410b8.
Stage base85a4c34790058d39b2cc8832f794a9718025d51c (accepted Part26D).

## Delivered boundary

Private OwnJsonReader in Formats parses a complete JSON document iteratively,
owns its source/decoded bytes and indexed nodes, retains exact numeric lexemes,
validates scalar Unicode, and applies syntax-before-BFS resource diagnostics.
Input cap1,048,576 bytes; resource limit4096 values; container depth32. Failed
results publish no document; owned result paths can contain embedded zero.

The source uses its own private header and C++ standard headers. Unit tests run
without a reference engine. Only the Telegram differential test consumes pinned
parser headers. Formats still contains the pre-existing miniz TGS transport:
this is not a claim that the entire target has no third-party code.

Current NativeEllipseAdmission/Input, Runtime routing, Model, Player and rendering
ownership are unchanged. This is a reader/value prerequisite, NOT a full Lottie
loader, reference-free playback, native raster/GPU path, or host integration.
It does not activate the own reader in the production Runtime.

## Commits and independent review

- ac53abc: reader design;06a64d4: detailed plan/preflight.
- 331765f: bounded reader and direct tests;2c2ece7: independently reviewed handoff.
- d11a249: independent pinned-policy comparison;88ee923: reviewed evidence handoff.
- a98d588 and9212c03: sole final test fix wave, append-only. Exact diagnostic
  distinctions, bounded root reachability and all published-success call sites.

Task1 and Task2 each passed independent spec/quality review. Whole-stage review
85a4c34..88ee923 found no Critical defect, one Important undocumented diagnostic
policy difference, and one Minor reachability-check gap in both test helpers.
The controller approved a written spec addendum before the single combined fix
wave. One completed scoped re-review over88ee923..9212c03 addresses both findings
with no new breakage. Final platform suites and supplemental header/link/install
closure pass; no Critical/Important or untriaged Minor finding remains.

Review packages/reports remain in .superpowers/sdd/2026-09-24-own-json-reader/.
Raw evidence remains in out/part26e/, with earlier failures retained separately.

## Differential and functional evidence

Final focused test revision9212c03: direct80743 checks; independent comparison
95798 checks,512 generated valid documents and1024 all-byte/delimiter mutations,
1582 same-policy observations and24 explicit intentional differences. Counts are
assertion/case observations, not independent feature counts or exhaustive fuzzing.
Both sides compare codes, length-aware paths and complete ordered owned values.
Literal expected rows and12 mutation witnesses protect against two observers
agreeing incorrectly. Original fixed seed and all-byte deletion remain intact.

Three deliberate private-policy distinctions are documented and tested:

1. Lexical numbers, including huge exponent spellings, do not inherit the pinned
   binary parser's inconsistent overflow eligibility. Exact token bytes remain.
2. Unicode scalar validation rejects lone escaped low surrogates accepted by the
   pinned parser. Generated case184 asserts exact input identity plus independently
   literal complete legacy values; it is an executed witness, not a whitelist.
3. Empty ancestor names retain pointer segments: own `//a` versus old `/a`, with
   duplicate-empty, depth and count cases, `/outer//a` control and malformed suffix
   precedence. The legacy helper and oracle have not been silently repaired.

The whole-stage probe independently reproduced the path distinction. A focused
functional test failed the prior equality assumption. Both graph helpers first
failed a synthetic disconnected-cycle test against parent-count-only checks,
then passed after bounded root traversal. Invalid edges/repeated siblings and a
valid-tree control are exercised; concurrent checks do not race the global counter.
All RED/GREEN outputs are under out/part26e/final-fix/ with commands and source hashes.

## Logical storage observations

The direct tests exercise the full byte cap: broad and deep inputs each produce
524288 nodes (capacity524288); cap-deep frame peak524287/capacity524288;32-byte
nodes and20-byte frames. Resource queue never exceeds4096. Large string retains
1048574 decoded bytes; huge exponent remains one lexical token. Malformed syntax
still wins before post-parse resource errors. Byte-cap+1 is rejected before copying.

These are actual logical size/capacity counters, not process peak memory or
throughput benchmarks. The design deliberately permits transient O(input) storage
for complete syntax before resource eligibility. No zero-allocation, speedup or
TSan claim is made; two independent reader threads are functional isolation tests.

## Final gate commands and evidence

Run under the installed x64 MSVC environment through retained scratch runners:

```text
out/part26e/run-preset-gate.cmd windows-msvc-direct2d 9212c03c1b4e60f8221932c19754964c7bd410b8
out/part26e/run-preset-gate.cmd windows-msvc-telegram-debug 9212c03c1b4e60f8221932c19754964c7bd410b8
out/part26e/run-preset-gate.cmd windows-msvc-win32-preview 9212c03c1b4e60f8221932c19754964c7bd410b8
out/part26e/provenance-gate.cmd 9212c03c1b4e60f8221932c19754964c7bd410b8
pwsh -NoProfile -File out/part26e/verify-boundaries.ps1 -EvidenceTag 9212c03c1b4e60f8221932c19754964c7bd410b8
```

Fresh configure/build/CTest: none32/32,4.36s; Telegram76/76,89.88s; Win32-preview
70/70,91.00s. Every registered test ran; zero failures/skips. Preview includes
capture, capture_preflight, contract, smoke and preview.selftest. Samsung configure/
registration and vendor-both-trees/TGS16 checks pass. Per-attempt success manifests
bind raw commands/exits, CTest XML/listing, graphs, product/source and instrument
hashes at the same HEAD. Docs remain dirty during gates; product inputs are stable.
All four manifests verify887 product/config hashes plus four instrument hashes.
Incremental build is intentional; actual CTest runs are fresh. Samsung is only a
configure/registration/provenance boundary, not a passing Samsung runtime suite.

Exact graph/source checks pass. Controller read complete archive/link blocks and
rules, source includes, CMake diff and actual generated install script: no reference
edge in reader/unit, differential only Telegram, private header not installed or
exposed, old route/tests unchanged. Formats retains its existing miniz transport.
Ninja's retained dependency records contain local headers but no standard headers;
this is not treated as exhaustive external closure. Supplemental syntax-only
MSVC traces captured98 reader headers (1 project/65 MSVC/32 SDK) and155 unit
headers (2 project/117 MSVC/36 SDK), zero unknown paths. Compiler/SDK roots were
observed14.44.35207 and10.0.19041.0. Exact commands match across all three presets
per source, so only two traces ran. Root independently verified canonical raw
path sets and every header hash. Graphs/original objects/887 inputs/four hashed
instruments remained unchanged. Report out/part26e/include-trace/report.md and
root9212c03...-manual-boundary-review.md record the actual evidence and limits.

## Retained limitations and earlier failures

Task1's resource routine was drafted before its RED, then withheld for compiling
functional RED and restored for GREEN. Behavioral evidence exists, but this does
not retroactively establish strict initial test-first chronology. Task2 early RED
and first discrepancy outputs are explicitly transcript-derived, not original
redirected files; final outputs and exact failing input/hex are retained raw.

An early full Telegram75/76 failure came from nested configure lacking compilers
outside VsDevCmd; subsequent task run76/76 under that environment passed. A proposed
unquoted-only mutation filter was rejected; full all-byte selection restored before
the final Task2 commit. Vendor deprecation warnings in the initial differential
build led to SYSTEM marking of test-only vendor includes, not vendor modification.

Scratch final-runner failures at88ee923 preceded configure/build: Windows PowerShell
Get-FileHash resolution, then multiple installed CMake command matches. The runner
now uses already installed pwsh and selects the first exact application path;
isolated version/JSON smoke passed. No policy bypass or Windows setting change.
Failed evidence is retained. The fix worker also initially named a nonexistent
none build tree; it corrected the path before the actual functional RED/GREEN.
The supplemental include runner initially rejected a scratch-only PDB emitted
with retained /Zi despite successful /Zs compilation. Evidence remains; completion
accepted that diagnostic artifact and ran only the missing unit trace, not a
duplicate reader compile. No build-tree output changed. Root's first raw-path
comparison found only two mixed slash spellings; canonical paths and hashes match.

Task3 prematurely marked its first report DONE before completing its call-site
audit. Root held final gates and interrupted the not-yet-completed scoped review;
the follow-up stayed in the same wave as a new commit, never amend. Final review
uses the complete88ee923..9212c03 range; no intermediate review is counted twice.

The whole-stage review's declined-scope items are explicitly resolved: own
admission/model/timeline/pixels/UI and exhaustive Lottie coverage are later stages;
transparent legacy scalar compatibility is intentionally not this private API's
contract; exhaustive fuzzing/allocation-failure/process-memory/speed/TSan remain
unclaimed. Samsung runtime is not checked by its configure gate. Final vendor/UI/
link/install evidence is controller-owned closure, not inferred from a source diff.
Historical strict TDD chronology remains limited as disclosed above.

## Decisions made under delegated authority

1. Reader/value prerequisite before admission: independently proves ownership and
   parsing; cost if wrong is an extra integration stage, with no own playback yet.
2. Separately named lexical-number/scalar-Unicode policy: avoids silent old API
   replacement; cost is an explicit later compatibility/adapter decision.
3. Complete flat parse before bounded BFS: preserves syntax precedence; cost is
   transient O(input) storage, not a zero-allocation or process-memory guarantee.
4. Direct main/scoped pushes and preserved raw/SDD per user choice: cost is less
   isolation and retained scratch; remote guards replace rewriting/force operations.
5. Retain reviewed Task1 resource code with disclosed test-first lapse: cost is
   weaker chronology assurance; independent differential and final review required.
6. Restore all-byte mutations and pin exact case184 instead of narrowing coverage:
   cost is fixed-seed witness maintenance; new differences need explicit inventory.
7. Preserve empty ancestor segments in the own reader and document legacy quirk:
   cost is a third private compatibility distinction for later own admission.

## Continuation

Next: extract one compiled first-party exact-decimal/ellipse-grammar
core, connect the own document without pinned fallback, then own model/resources/
certificate, timeline and native pixels. Avelabs-UI remains the final static EXE
host, with isolated opt-in acceptance there after those engine prerequisites.

Latest read-only host check: clean712d454a7c5c175ad59a3ca547c6b22ce8392da1;
UI/out absent; accepted build/Release/AvelabsUI.exe SHA256
C92F26EE8FEB4FF03F6DC7F4AFFF4B11FB48142743D1EDCCB4E213AAA81F82C2 unchanged.
No UI build/GUI, vendor/fixture/license change, automation, dependency installation
or Windows setting change was made by E.
