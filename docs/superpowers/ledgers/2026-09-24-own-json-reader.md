# Part26E own JSON reader execution ledger

Plan: docs/superpowers/plans/2026-09-24-own-json-reader.md
Spec: docs/superpowers/specs/2026-09-24-own-json-reader-design.md
Stage BASE:85a4c34790058d39b2cc8832f794a9718025d51c (D final accepted/pushed).
SDD workspace: .superpowers/sdd/2026-09-24-own-json-reader/

## Authority and design — 2026-09-24

User requests long complete continuation and delegates reversible spec/plan/
execution decisions, main commits and ordinary push. Controller approved written
spec ac53abc and this plan after self-review; selected SDD. This is not a claim
that the user reviewed unseen artifacts. No new schedule, UI write or dependency.
D complete: final review has no blocking findings, final gates recorded in its
report, final85a4c34 ordinary-pushed with exact remote equality.

Existing first-party admission still uses pinned parsing; observations from73
general and36 numeric cases, plus read-only numeric design audit, justify a
separate reader prerequisite instead of an unproven transparent replacement.
Raw data/code/hash manifests remain under out/part26e-characterization and
out/part26e-numeric-characterization; these are probes, not E product tests.

## Rulings

1. Ruling: E builds the reader/value boundary only, leaving old admission and
   Runtime untouched — establishes own syntax/ownership/bounds independently of
   schema/model construction — cost if wrong: an extra staged integration step;
   this is not delivered own loading/playback.
2. Ruling: the separately named private reader uses lexical numeric tokens and
   Unicode scalar strings, not pinned binary-conversion/lone-low-surrogate quirks —
   measured equal-number eligibility differences make silent replacement unsafe;
   no existing API changes — cost if wrong: future native ingress needs a different
   compatibility policy or adapter, with those decisions still unmade.
3. Ruling: complete iterative parse into flat indexed storage before bounded BFS
   resource diagnostics — preserves whole-syntax precedence and stable spans even
   for rejected inputs — cost if wrong: transient O(input bytes) memory, explicitly
   measured by logical capacities and not misrepresented as process peak memory.
4. Ruling: use direct main/scoped pushes and retain raw/SDD under user choice —
   preserve append-only history and remote guard instead of worktree cleanup —
   cost if wrong: less isolation and retained scratch; no destructive cleanup.
5. Ruling: retain Task1's reviewed resource implementation and disclose its
   test-first sequencing lapse instead of rewriting history or claiming strict
   chronology — worker acknowledges drafting that routine before RED; actual
   compiling stub RED/restored GREEN and independent review establish narrower
   behavioral evidence — cost if wrong: weaker process assurance for that block;
   Task2 independent oracle and final review remain required, no retrospective
   claim that the original resource implementation followed test-first order.
6. Ruling: restore original all-byte deterministic deletions and pin observed
   case184 as an exact executed scalar-policy witness, not narrow mutations to
   unquoted bytes — retained input shows deletion of the first surrogate escape's
   backslash, leaving literal uD83D plus escaped lone-low DE00, already a specified
   reader-policy difference — cost if wrong: a brittle named seed witness requires
   maintenance when generator design changes; it must assert exact input identity
   and independently expected full legacy values, never auto-whitelist mismatches.
   Additional witnesses require retained inputs and an explicit bounded inventory.
7. Ruling: retain every empty ancestor segment in the private own reader's paths,
   with explicit own/legacy diagnostic witnesses, rather than copy the legacy
   root-marker ambiguity or silently repair the oracle — focused review probe
   independently confirms duplicate/depth/count differences in unchanged old
   admission; cost if wrong: a third private compatibility distinction, which
   later own admission must preserve or explicitly adapt. No old API change.

## Preflight

| Tasks/interface | Producer/consumer or self-check | Finding/resolution |
| --- | --- | --- |
| Task1/Task2 OwnJsonReader API | Exact node/result/document/statistics signatures consumed by independent observation | Shared names and index semantics match; Task2 cannot call product helpers for expected output. |
| Task1/Task2 CMakeLists.txt | All-variant Formats/unit versus Telegram-only oracle target | Sequential writer; no vendor link/include on Formats or none unit target. |
| Task1/Task2 reader behavior | Explicit scalar policy versus pinned eligibility | Deliberate differences are named/count separately; unchanged old admission is not substituted. |
| Task1 self | Functional baseline/resource RED, complete owned values, stress/Unicode/lifetime | No schema/float conversion; source cap before copy, syntax before BFS, iterative destruction; stats retained even on failure. |
| Task2 self | Independent DOM/SAX observations, actual known token outcomes, deterministic generation | Full16 token constructions included in brief; no runtime out-file dependency;512+1024 generated cases with fixed96-node budget and8KiB cap. |
| Both/constraints | Private/all-variant reader versus no public route/vendor/UI mutations | Header stays src; root full gates and source/link inspection establish scope. |

Spec coverage checked: all reader scope/ownership/scalar/resource clauses Task1;
independent comparison/policy differences/generation Task2; fullplatform/provenance/
legacy API preservation root. Five review focus cases have explicit test matrices.
No placeholders found. Plan type/value examples checked (observation root/object/
array+3 scalars=5 nodes). No existing product writer; .git equals git-common-dir,
main selected explicitly; no worktree required under user override.

## Tasks

- [x] Task1 owned reader and direct tests. BASE06a64d47a250ee532149578bbcb78c42a24f7051; product331765f, independent spec/quality Approved, no code findings.
- [x] Task2 independent comparison/policy evidence. BASE2c2ece7b689e93dbba7795ea82a59c1cbfee1d00; productd11a249, independent spec/quality Approved, one deferred Minor.
- [x] Root final gates, whole-stage review and report. Guarded ordinary push follows scoped docs commit.
- [ ] Follow-on own admission/model connection design; not part of E reader completion.

Controller owns STATE/docs/ledgers; one worker owns product files at a time.
Minor findings must be listed here for final triage; none yet.
Spec/plan/preflight06a64d4 ordinary-pushed, exact remote equality/clean tracked tree
verified before Task1 dispatch. Read task-1-report.md when the worker finishes;
do not run shared builds concurrently or start a duplicate writer.

## Task1 verification and review checkpoint

Product331765fb7012f67f47bf0eb23471654fe27019fb is committed locally, not pushed.
Four-file scope checked; only controller STATE/ledger remain dirty. Functional
reader stub RED preceded syntax implementation. Resource gate has a process
deviation: initially drafted before its RED, then removed for successful-build
functional RED and restored for GREEN. Preserve that distinction; no retroactive
claim of strict test-first development for the gate. Independent Task1 review
and Task2 oracle checks remain required before stage acceptance.

Worker final none32/32,4.19s; Telegram scoped4/4,1.27s, no compiler warnings in
these logs. Direct34288 assertions; node/frame sizes32/20 bytes, cap-deep frame
peak524287/capacity524288; queue never above4096. These are logical storage
observations, not process-memory/performance/TSan claims. Root fresh committed-code
reader1/1,0.69s at out/part26e/331765f-task1-root-focused.log. Review package
review-06a64d4..331765f.diff and task-1-report.md consumed by task reviewer
/root/own_json_reader_task_review. Task2 brief extracted, not dispatched.

Read-only future connection recommendation: out/part26f-design/admission-connection-audit.md;
not an approved F design or product change. Separate scratch gate-instrument audit
out/part26e-design/gate-instrument-audit.md found evidence-coverage gaps, not a
demonstrated product leak. Its author is refining only root scratch instruments:
complete link blocks, actual include dependencies, exact source/test identities,
revision/graph binding, preview and protected/install checks. No gate execution
until Task2 settles; Samsung remains configure-only in this stage.

Task1 review accepted both spec and quality, zero Critical/Important/Minor code
findings. Process exception resolved by Ruling5, not erased. Controller checked
four-file scope/CMake diff: only own Formats source/test and shared Threads
discovery changed, no install/vendor/runtime/fixture edit. Fresh UI check remains
clean712d454, UI/out absent, accepted EXE SHA256
C92F26EE8FEB4FF03F6DC7F4AFFF4B11FB48142743D1EDCCB4E213AAA81F82C2 unchanged.
Full stage graph/provenance/Telegram/preview remains explicitly pending, not a
Task1 completed claim. Remote main was exactly06a64d4 before this scoped handoff;
ordinary push follows commit. Task2 may start after successful push verification.

Task1 handoff2c2ece7b689e93dbba7795ea82a59c1cbfee1d00 ordinary-pushed; exact remote
equality and clean tracked tree verified. Task2 dispatched to fresh sol/high
/root/own_json_differential_implementation with extracted brief and no history.
Exact parser-policy differences remain requirements; prior process exception is
not permission to repeat implementation-before-RED. No concurrent product writer.

Scratch gate refinement handed off without executing shared gates. Controller
read both full PowerShell scripts and wrappers, checked actual required CTest
names against existing preview registrations and performed AST checks (zero
errors). Read-only helper smoke resolves current HEAD, none cache variant and
six graph paths; not a build or final acceptance. A draft execution-policy bypass
flag was removed before any invocation; no policy/security setting was changed.
The final runners now capture exact command exits, per-attempt raw/JUnit/graphs,
stable HEAD/source/config/instrument hashes, and actual Ninja dependencies;
complete link/header/install manual review remains explicit in final JSON.
Usage/caveats: out/part26e-design/gate-instrument-audit.md. Do not alter these
instrument files during/among the final identity-bound gate sequence.

## Task2 intermediate diagnosis — not final acceptance

Original successful-build functional RED and initial discrepancy outputs were
captured only in the worker tool transcript, not redirected files. Worker now
retains explicitly labeled transcriptions with exact commands/exits, not invented
original raw logs. Fresh final commands must redirect their raw outputs. Original
transcript remains the provenance; no history rewrite or RED replay to conceal it.

Generated case184 original deletion witness is preserved as .raw/.hex under
out/part26e/task2/failures. Root read hex and narrow generator hunk; worker confirmed
exact cause. A proposed unquoted-byte-only mutator narrowed planned coverage and
was rejected before commit (Ruling6). Restoring original selection/RNG and adding
full independent literal legacy rows is required; no product parser fix indicated.
Interim1579/19 counts and narrowed-generator passing runs are not final evidence.

First full Telegram attempt75/76 failed only avemotion.cmake.legacy_subproject:
its nested configure could not find C/CXX compilers because CTest ran outside
VsDevCmd. Root read actual error and summary. Retain telegram-full-ctest.log;
rerun in installed x64 compiler environment, no Windows change/test weakening.
Further full runs wait until final generator correction; focused tests for iteration.

Task2 productd11a24952837b8892a6e0ca313d658291e500d4c is local, not pushed. Worker
DONE_WITH_CONCERNS (early transcript-only RED retention and diagnosed environment
failure), no product source changes. Restored original all-byte deletion executes
512 valid+1024 mutation cases,1578 same-policy/20 explicit differences,94955 checks;
case184 asserts fixed bytes and five independently literal legacy rows. Final
focused2/2 and full Telegram76/76,92.81s under VsDevCmd pass. Independent reviewer
/root/own_json_differential_task_review sees brief/report/review-2c2ece7..d11a249.diff
and Ruling6; Task2 not complete until accepted. Three-file scope, only controller
STATE/ledger dirty; no live writer/build. Root final platform/evidence gates pending.

Task2 independent review is spec compliant and quality Approved, no Critical or
Important findings. Root fresh committed-code reader/differential2/2,0.83s at
out/part26e/d11a249-task2-root-focused.log. Task1 behavior was separately reviewed;
Task2 CMake changes only the Telegram test and does not alter Formats dependencies.
Final none/Telegram/preview/provenance/include/link checks remain stage-owned.
Remote exactly2c2ece7 before scoped task handoff commit/push.

Task 2: minor (deferred): differential index checker counts parents but does not
prove root reachability of every arena node; a disconnected cycle could pass.
Location tests/own_json_reader_differential_tests.cpp:94-105. Reviewer calls this
a coverage gap, not an observed reader defect. Whole-stage reviewer must explicitly
triage it; no minor-only task fix loop and no silent dismissal.

## Whole-stage review and sole combined final fix wave

Task2 docs88ee923 ordinary-pushed; exact remote equality/clean tree verified.
Whole-stage review85a4c34..88ee923: no Critical; one Important undocumented
empty-ancestor diagnostic-path distinction and one Minor reachability gap in BOTH
test helpers. Report final-review.md, original scratch probe under
out/part26e/review-empty-ancestor. Controller verified source/probe and approved
the explicit spec addendum above under delegated authority (Ruling7). Both findings
enter the sole combined test fix wave, Task3, FIX_BASE88ee923. Production reader,
old admission, and oracle remain untouched. No final platform success claimed yet.

Scratch final-gate startup failed before configure/build first under WindowsPS5.1
Get-FileHash resolution, then after installed pwsh selection because Get-Command
returned two application paths. Corrected scratch helper selects the first exact
executable and records command/launch failures; isolated version/JSON serialization
smoke passed, earlier failure files retained. No Windows/policy setting change.
Fresh final gates will run only after the final fix/re-review, at a stable SHA.

Task3 initiala98d588 focused none1/1/Telegram2/2 passed; report was marked DONE
before implementer quiescence. The worker then identified success paths not calling
the strengthened check in both test units. Root held all final gates, interrupted
the just-dispatched scoped reviewer, and required an append-only follow-up commit,
not amend/history rewrite. This stays one fix wave/one completed scoped review
over original88ee923..finalHEAD. Wait for actual final report/event before execution.

Task3: complete (commits88ee923..9212c03, scoped review clean). Writer explicitly
quiescent; one completed scoped review resolves Important path policy and Minor
reachability, no new breakage. Final focused none1/1/Telegram2/2; direct80743,
differential95798 checks with512+1024 cases/1582 common/24 differences. Both raw
RED cycles and append-only call-site follow-up retained. Root fresh full none
32/32,4.36s and Telegram76/76,89.88s at9212c03 pass; preview/provenance/boundary
closure remains pending. Draft final report is not yet acceptance.

## Final acceptance at9212c03

Fresh final none32/32,4.36s; Telegram76/76,89.88s; Win32-preview70/70,91.00s,
zero skipped/failed. Samsung configure/registration, all-vendor/TGS16 and exact
boundary verifier pass. Manual complete link/rule/install/source reading confirms
private reader/test scope and no route activation. Supplemental syntax-only include
traces98 reader/155 unit headers: expected local headers plus installed MSVC/SDK,
no unknown path. Root verified canonical raw sets and every header hash. The two
trace commands are identical across presets per source; no full-suite duplication.
Scratch PDB assertion failure and slash-normalization comparison are retained and
explained, not product regressions. Final manifests bind887 product/config inputs,
four gate instruments and generated graphs; all unchanged through supplemental
trace, including original compiled objects. No final product patch after9212c03.

Root resolves final review's declined-scope items in final report
docs/PART26E_OWN_JSON_READER_REPORT.md: future own admission/model/playback/UI,
finite conformance/memory/performance limits, intentional scalar/path distinctions,
Samsung configure-only and historical TDD evidence limits are explicit. No open
finding. All seven Rulings/costs are in the report. E accepted; remote still88ee923
before scoped docs handoff/ordinary push. Continue F design after push verification.
