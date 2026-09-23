# Part25I — strict private native-ellipse input admission

Status: locally complete and independently approved. Product is ae72a8b, followed
by the reviewed test-only correction82e39a7. Fresh full Windows/none gates pass
on that product; both affected test profiles also pass after the test correction.
The user requested wrap-up, so no next stage or automation is started. Ordinary
push is the final handoff action; its CI is not certified by these local results.
No native playback route is enabled.

## What this stage changes

A private Telegram Runtime audit checks the complete raw JSON against a narrow
one-layer, one-group ellipse/fill grammar. It rejects unknown or duplicate fields,
extra content, malformed encoding, resource-limit violations and unsupported
types, values or structure. Rejections are internal input ineligibility, not a new
public load error. The result owns its diagnostic path.

The implementation is called only by its new test executable. It does not route
Asset loading, model publication, scene evaluation, CPU rendering or fallback.
Samsung and no-reference Runtime do not acquire the parser. Public headers,
vendor sources, corpus, goldens, licenses, primitive ownership, Player threading,
Direct2D ownership and ANGLE/backend coverage stay unchanged.

Binding design and plan:

- `docs/superpowers/specs/2026-09-23-native-ellipse-admission-design.md`
- `docs/superpowers/plans/2026-09-23-native-ellipse-admission.md`

## Initial evidence and correction

Task1 product commit334782be91b2d83eaf18e7fbc004328d9a85d9cf contains exactly the
private header/source, test and CMake wiring. Functional reject-all RED preceded
implementation. The initial focused test passed1/1 and runtime/validation20/20.

Independent review nevertheless compiled real false-acceptance probes: width
`1.0000000000000001`, frame rate `240.00000000000001` and fill alpha
`1.0000000000000001` all rounded into accepted DOM doubles. Root independently
reproduced the width/rate defect. The distinct lower-alpha value
`0.99999999999999999` rejected and is not claimed as a reproducer.

Amendment1cd73a5 retains exact mathematical comparison and allows a bounded second
metadata pass through the same pinned RapidJSON reader to capture original number
tokens. It adds no parser technology, dependency or public route, and permits no
implicit precision/exponent cap. The correction history is recorded below.

Round1 commit42489100c3152be5ae9d8781add1ed97d3199166 fixes those three cases.
Its scoped review then exposed a separate downstream defect: validated integers
were still extracted from rounded DOM doubles. Exact animation flag1 could
truncate to0 and admit static position syntax, while an equivalent rootop61
could truncate to60 and reject correct keyframe timing. Reviewer and controller
both reproduced these cases. Round2 retains the same contract, with separate
functional RED logs for the incorrect branch and timeline. No earlier passing
suite is presented as approval of either defect.

Round2 commitae72a8b45549fbc77b6d590d0c0aff200a55d4ad derives downstream integers
from already validated exact decimals, with bounded integer arithmetic. The audit
has no remaining GetDouble conversion. Focused1/1 and runtime/validation20/20 pass.
Independent scoped review accepted all findings with no new Critical/Important
issue. Controller full gates on the corrected code are listed below.

Pre-fix configure/build/full CTest results at334782b:

| Preset | Tests | Elapsed | Meaning |
| --- | --- | --- | --- |
| windows-msvc-telegram-debug | 69/69 | 87.63s | Ordinary suite, missed exact-decimal defect |
| windows-msvc-win32-preview | 63/63 | 88.87s | Includes WARP/capture/preview, not numeric sign-off |
| windows-msvc-direct2d | 31/31 | 4.27s | No-reference boundary remains intact |

Those earlier results did not override the verified defect. Logs, RED evidence,
reports and ledger are retained in
`.superpowers/sdd/2026-09-23-native-ellipse-admission/`.

## Fresh corrected-code gates

Controller runs at product commit `ae72a8b45549fbc77b6d590d0c0aff200a55d4ad`:

| Preset | Fresh configure/build | Full CTest | Elapsed |
| --- | --- | --- | --- |
| windows-msvc-telegram-debug | PASS | 69/69 | 82.94s |
| windows-msvc-win32-preview | PASS | 63/63 | 84.62s |
| windows-msvc-direct2d (none) | PASS | 31/31 | 3.37s |

Every native configure/build/CTest exit was zero. Commands were run directly
under the installed VS2022 VsDevCmd environment, for each listed preset:

```text
cmake --preset PRESET
cmake --build --preset PRESET --parallel 4
ctest --preset PRESET --output-on-failure
python scripts/verify_vendor.py --variant all
python scripts/generate_tgs_compatibility_corpus.py --check
git diff --check
```

The explicit preview preset includes Direct2D smoke/WARP, capture, preview
selftest and capture preflight/device-recreation coverage. The locally blocked
preview PowerShell runner was not executed or unblocked, and no Windows policy
was changed. The direct CTest gates are not claimed as local execution of that
runner.

All-vendor verification and the unchanged 16-asset corpus pass. Protected/public/
reference/backend/fixture/golden diffs are empty for this stage. Call-site search
finds only the private declaration/definition and tests. Generated build graphs
contain the admission object in Telegram/preview, but not in the none variant.
No Samsung feature or full-Samsung-green claim is made.

Raw corrected-code files use `controller-round2-ae72a8b-*`, including copied
CTest LastTest logs. Independent task reviews are `task-1-review.md`,
`task-1-numeric-fix-review.md` and `task-1-round2-review.md`. Earlier outputs remain
preserved, explicitly superseded where a subsequent review found a defect.

## Final review and handoff

Whole-stage review871bb86..7f52b6d found no product/Critical/Important issue and one
P3: the purported fill-opacity mutation actually changed layer opacity. Test-only
commit `82e39a7733c004c699e49a1c3ab105bfa8db386b` anchors the fill field uniquely
and asserts `/layers/0/shapes/0/it/1/o/k`. Its functional RED reported the old
`/layers/0/ks/o/k` path; focused rebuild/CTest then passed1/1 each in Telegram and
preview, native exits0. Full suites were not repeated for this test-only change;
the product code remains exactlyae72a8b. Raw prefix `task-1-final-wave-*`.

Independent scoped final review7f52b6d..82e39a7 closes the P3, with no new issues
and zero open findings. Reports `final-review.md` and `final-fix-review.md` are
preserved in the SDD directory. All declined-to-judge items remain explicit
non-goals listed below, not waived prerequisites for future native adoption.

Product history:334782b initial private audit,4248910 exact numeric comparisons,
ae72a8b exact structural integer outputs,82e39a7 precise fill-opacity regression.
Root documentation checkpoints include7f52b6d and this final handoff. No force
push, history rewrite, settings change or dependency installation was used.
Post-push cloud state, if available at wrap-up, is retained as
`controller-handoff-cloud.json`; an in-progress run is not an all-CI-green claim.

## Limitations and subsequent work

This is an input grammar prerequisite, not a native-renderer-ready certificate.
Part25H's1008/1008 throwaway pre-model comparisons do not prove model/planner/
history behavior, arbitrary input support, final module readiness or speedup.
Native playback requires separately designed raw-to-model correspondence,
scan-time immutable binding verification, model-applied scene/plan/history
parity, retained-result ownership, two-instance/CPU isolation and proof of no
per-frame production reference sampling. No benchmark is run in Part25I.

G's actual cloud preview job107183082122 passed on run35861658145; that is not an
all-CI-green claim. Separate Linux reference/ASan failures remain unresolved.
The canceled15-minute automation is not recreated.

## Delegated decisions and costs

- Isolate raw admission as its own stage, with no playback or primitive-ownership
  change. The cost is an unused private object until a later compiler adopts it.
- Treat Accepted as grammar-only, not renderer readiness. If that distinction
  is lost, premature native routing would be incorrect; later parity gates remain
  mandatory.
- Reuse the pinned private RapidJSON dependency and the explicit iterative/UTF-8
  parser. Input bounds limit memory, but there is no zero-allocation or licensing
  conclusion.
- Keep exact mathematical numeric semantics. The cost of the corrective design
  is a second cold metadata parse and private decimal normalization, not a new
  dependency or frame-time route.
- Correct the plan's mistaken AVEMOTION_BUILD_TESTS name to existing BUILD_TESTING.
  No new option is introduced; actual variant builds verify test selection.
