# Part26B durable ledger

Plan: `docs/superpowers/plans/2026-09-24-owned-native-ellipse-input.md`
Spec: `docs/superpowers/specs/2026-09-24-owned-native-ellipse-input-design.md`
Base: `350888f2cd762c1c91dcc2e6b6e32d78fe1a619f` (clean main/origin).

- Ruling: choose an exact owned descriptor before model correspondence — the
  strict audit currently discards accepted values, including positive underflow
  decimals; this isolates the input-side proof — costs a separate subsequent
  correspondence stage if combining would have been safe.
- Ruling: approve written spec/plan and SDD under the user's explicit delegated
  reversible decisions, work directly in main — avoids routine reapproval and
  honors prior execution choice — costs rework if the chosen small boundary
  differs from intended priority; no claim of user review of unseen documents.
- Ruling: keep parser Telegram-private and playback unchanged — none packaging
  and exact-to-float correspondence are not solved by a descriptor — costs no
  immediate reference-free playback, deliberately disclosed.
- Ruling: preserve SDD/raw evidence and leave completed heartbeat paused — user
  required evidence retention and latest continuation is interactive — costs
  disk space and no automatic future wakeup.

Read-only ingress/independence audits complete in `out/part26b-native-design/`.
Spec and plan controller self-review complete; one product task pending.

Task1 complete: product3238191, functional RED and GREEN2/2, full worker70/70
(85.59s); independent reviewer accepted spec+quality with zero findings.
Root fresh sequential gates: Telegram70/70 (82.41s), Windows preview64/64
(88.64s), none Direct2D31/31 (4.86s). Vendor/TGS16 passed. Fresh generated
none/Samsung graphs exclude private parser; none Runtime link graph excludes
reference libraries/objects/includes. Review's external-evidence item resolved.
Whole-stage review pending, no product writer. Report under docs/PART26B_*.
No UI changes, no automation changes; preserve SDD workspace/raw logs.

| Preflight pair/task | Producer / consumer / consistency | Result |
| --- | --- | --- |
| Task1 internal | New private declaration and exact copy implementation; tests use same field/type contract | Consistent; audit pipeline ordering unchanged |
| Task1 -> final gate | Committed private object/tests; full presets and no-ref graph consume unchanged CMake boundary | Consistent; final stage does not activate callers |

UI coordination: separate UI task owns its out cleanup/build paths. No UI
writer/build/GUI from this stage. Unique Part26A diagnostic evidence is being
archived outside UI/out before ACK; new host path handoff is requested.
Archive complete at `out/part26a/ui-evidence-archive-2026-09-24`:112 files,
1,707,923 bytes,112/112 SHA256+size matches. Manifest SHA256
`1f6c9185eabd8429f96c2fce2aa9862d174b4f148e5a74d174823c585b0aae72`.
UI task received ACK quiescent. Compiled products/dependency trees are excluded;
README and manifest document exact scope; canonical measurements remain local.
