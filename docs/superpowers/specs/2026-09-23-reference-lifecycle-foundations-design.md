# Part25B — reference lifecycle foundations

Date:2026-09-23. Base:273f2b3. Status: approved under the user's renewed
delegation to decide and work independently for two more hours.
Deadline:07:11:24UTC /10:11Moscow; reserve the last20minutes for closeout.

## Intent and bounded outcome

Continue making AveMotion a lean Windows-hosted animation module, without
changing its visual contract or introducing a new graphics ownership model.
Part25A proved that persistent renderTree reuse is unsafe. That evidence stays
binding; this stage does not attempt the broader3–7hour recording redesign.

The selected bounded optimization removes the ordinary Instance's retained
upstream tree that is used only to convert a normalized position to a frame.
It reduces construction/retention work, not the per-frame fresh-sampling cost.
A separate bounded probe turns previously unexecuted active epsilon-state
risks into reproducible evidence and small future-reuse regression fixtures.

## Alternatives and choice

1. **Metadata mapping for safe counts, legacy outlier path — selected.** No
   redundant tree construction for ordinary assets; exact pinned rounding is
   maintained and malformed/outlier acceptance is not silently redesigned.
2. Construct then discard the tree at createInstance. This preserves the extra
   load's failure opportunity/counter but retains startup cost and requires new
   lifetime telemetry to demonstrate release. Not selected.
3. Implement all recording-mode topology/scratch/publication changes now. The
   prior assessment exceeds this window and has unproven caches. Deferred.

## Mapping contract

Let N be the already validated asset metadata totalFrames. For 1 <= N <=
numeric_limits<long>::max(), map using immutable metadata only:

- Normalize any nonfinite input (NaN, either infinity) to0, matching existing
  Instance behavior. Clamp finite input to[0,1].
- N=1 returns0.
- Telegram truncates p*double(N-1) to size_t.
- Samsung uses size_t(std::round(p*double(N-1))). Do not replace with floor(x+.5).
- Retain the final clamp to N-1. Frame rate and duration are not used.
- Choose the rounding policy by a private compile definition derived from the
  pinned CMake variant, not by the parsed-model feature flag or a string lookup
  on every call. There is no new public API.

The upstream source formulas are in each variant's lottiemodel.h. Telegram root
ip/op parse truncates into long; Samsung rounds. Using the resulting N preserves
those differences and nonzero/negative root offsets. Differential tests use the
independent ReferenceAnimation API, with nonfinite inputs normalized before
calling that oracle because its wrapper lacks Instance's finite check.

For N > LONG_MAX, keep the existing eager animation load and old frameAtPos
mapping. Telegram can accept reversed ranges whose signed span wraps into a
huge size_t; deriving its old signed behavior from unsigned N is not justified.
The outlier path preserves it without adding new asset rejection. Do not test
or promise a portable value for old out-of-range float-to-integer conversions;
test accepted reversed metadata, legacy session selection and position0 only.
Broader malformed-timeline validation is separate work.

## Ownership, counters and errors

Ordinary createInstance retains no mapping animation and creates zero Scene-role
sessions. Rename the optional retained member to make its legacy-mapping purpose
clear. Outlier createInstance still creates/retains one Scene-role animation.
Asset load still validates through a Metadata-role animation before publication.
Null/foreign assets and no-reference behavior remain unchanged.

Removing an operation intentionally removes its failure opportunity: ordinary
createInstance no longer performs a second upstream parse/construction that
could return InstanceCreationFailed or throw while allocating renderer objects.
Do not claim literal preservation of that obsolete branch. Its enum/category
and the outlier branch stay, as do existing asset-load and evaluation errors.
The audit finds no new validation in that duplicate immutable-JSON load under
ordinary operation; cache corruption/resource failure is not simulated away.

Fresh exact sampling remains one Scene-role session per actual sample. Model
preparation remains fresh per source frame. The lazy isolated CPU animation is
unchanged. Instance confinement, shared asset/model synchronization, handles,
Player scheduling, visual scenes and public fallback policy are unchanged.

The six diagnostics remain successful-creation/sample counts, not live gauges.
All ordinary corpus assets now report setup_scene_sessions=0 instead of1.
First/steady counts and schema2 instrumentation are unchanged. The output
validator gains an explicit expected-setup-count option so old baseline data can
still be validated with1 and new data with0; do not weaken the assertion.

## Proof and acceptance

Before product edits, an actual test must fail because ordinary creation or
mapping constructs a Scene session. Parity covers all16 corpus inputs, dense
positions/nextafter boundaries, literal truncation-versus-rounding ties,
single-frame/short timelines, fractional/nonzero/negative root endpoints,
nonfinite/out-of-range inputs, reset epochs and move/independent instances.
Scene/model/CPU/access-order tests retain their visual assertions; only the
intentional setup count changes. Check null/foreign error codes as well.

The active-state scratch probe must compare immediate deep copies on both pinned
variants and retain passing controls. Promote a bounded subset of its small
self-authored fixtures under tests/fixtures/reference_sessions (never top-level
auto-enumerated corpus inputs). Fresh production scenes must match the reference
and independently recorded critical fields; a temporary, restored mutation or
the actual direct-session probe must demonstrate the specific failure caught.
No unsafe recording fix is shipped merely because some cases pass.

No vendor sources, patches, notices, goldens or acceptance whitelist change.
No dependencies/Windows settings, ANGLE, D3D9/D3D11on12 or graphics ownership
change. Samsung's existing scene/plan endpoint failures remain disclosed and
must match clean-baseline hashes, not be skipped or relaxed.

## Measurement and delivery

Retain a baseline Release executable before rebuilding; verify its hash and
unchanged instrument/runtime lineage. Use the same corpus instrument for fresh
A1,B1,B2,A2 processes, samples1000/warmup20/CPU0/128square. Use fresh processes
in that order for1/16/64-instance working set, for both StickAndBall and firework
if time permits. Collect no timings while builds/tests/probes are running.
Counter reduction and memory observations are primary outcomes; unchanged
per-frame sampling means no2x exact-scene claim is expected. Keep raw numbers,
input hashes and compiler/binary identities. No zero-allocation or TSan claim.

One product implementer at a time; task reviews, one final whole-change review,
fresh four-preset MSVC/CTest and installed-consumer gates, small ordinary main
commits/pushes. User delegated design/plan/method approval; no routine checkpoint
is pending. The deadline limits new work, never permits unverified shipping.
