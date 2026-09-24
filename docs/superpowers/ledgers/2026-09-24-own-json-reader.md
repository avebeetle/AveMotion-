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

- [ ] Task1 owned reader and direct tests. BASE/agent assigned after docs push.
- [ ] Task2 independent comparison/policy evidence.
- [ ] Root final gates, whole-stage review, report and ordinary push.
- [ ] Follow-on own admission/model connection design; not part of E reader completion.

Controller owns STATE/docs/ledgers; one worker owns product files at a time.
Minor findings must be listed here for final triage; none yet.
