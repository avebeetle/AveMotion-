# Part26F — own ellipse admission connection design

Part26E is accepted and ordinary-pushed at7000dd0b7470a17fd21ad3515a44eb7f140adae9;
its tested product revision is9212c03, final none32/32, Telegram76/76 and preview70/70.
No E task is repeated. User delegates written design/plan/SDD decisions and ordinary
main pushes. This document is controller-approved under that delegation, not a
claim that the user reviewed unseen artifacts. Implementation requires its detailed
plan and preflight, with one product writer.

## Intent, scope and destination

The user's goal is an own Lottie/TGS animation engine linked statically into the
future Avelabs-UI EXE, with independent correctness evidence. This stage connects
the accepted first-party JSON reader to the existing narrowly supported Part25I
ellipse grammar and Part26B fully owned descriptor. It removes the pinned parser
from that separately named input path, not from today's production Runtime.

Success: the no-reference build can consume a successful own JSON document and
return every field of NativeEllipseInput with independently specified values;
existing Telegram audit/decode entry points retain their old code/path/eligibility
and descriptor contract. No own Model, certificate, timeline, pixels or host route
is delivered here. No general Lottie feature expansion is permitted.

Avelabs remains D:/rvc/c++/DragonianVoice/Avelabs-UI. No UI writes/builds/GUI or
UI/out recreation in F; preserve accepted build/Release EXE and Motion Lab's
default-OFF, no-install, experimental reference CPU status. Native host playback
is a later separately verified step, not a name change to the current lab.

## Architecture choice

Choose one compiled, private first-party grammar/decimal/materializer core fed by
a small borrowed value table. Old pinned DOM+raw SAX and own immutable E document
are separate front ends. The table preserves ordered child links and lexical
number bytes without vendor types. Maximum4096 nodes follows successful ingress
resource validation. Cost is a bounded extra projection/storage step; no speedup,
zero-allocation or process-memory claim.

Alternative: templated cursors avoid table projection but compile grammar twice
and enlarge adapter/template surface. Rejected for this stage's simplicity, not
because measured slower. Copying grammar duplicates rules; reserializing through
either parser loses numeric spelling or repairs/rejects legacy surrogate bytes.
Neither is allowed. No general DOM framework, plugin registry or mutable public
JSON builder is needed.

## Responsibilities and files

- NativeEllipseAdmission.cpp remains the Telegram-only compatibility front end:
  exact original byte/NUL gates, pinned parse flags, BFS duplicate/resource walk,
  raw-number SAX and full event-kind alignment, then table projection/core call.
- NativeEllipseAdmissionCore.hpp/.cpp are private Runtime sources: tiny borrowed
  table and one compiled implementation of existing exact decimal arithmetic,
  ordered ellipse grammar and descriptor materialization. No parser/vendor type.
- OwnNativeEllipseAdmission.hpp/.cpp are private, all-variant Runtime sources:
  synchronous projection of a successful E document into the same core. No old
  decoder/auditor call, reparse, serialization or fallback.
- Existing NativeEllipseAdmission.hpp and NativeEllipseInput.hpp remain unchanged.
  Existing legacy admission/input tests remain unchanged as regression evidence;
  new core/connection tests supply independent literal expectations.
- CMake compiles core and own front end unconditionally in Runtime, with only
  private own Formats-header visibility. Legacy front end retains its Telegram
  guard. All-variant tests cannot include/link the legacy oracle or pinned headers.

## Minimal private contract

The core table has a first-party six-kind enum (Null, Boolean, Number, String,
Object, Array), uint32 node indexes with UINT32_MAX absent, root index0. Each record
has kind, hasKey, length-aware key and scalar byte views, firstChild, nextSibling,
childCount. Scalar bytes mean raw valid number token for Number, decoded bytes for
String, empty otherwise. Boolean payload is unnecessary: this grammar accepts no
boolean field. Empty key and absent key must remain distinct.

childCount counts immediate children; firstChild/nextSibling links, not contiguous
node ordinals, define ordered object members and array indexes. Nested descendants
may intervene between siblings in the flat table. Scalars have no children; root
has no key or sibling. Each projector maps edges to its table's own index space;
own-document indexes may be reused only when every row retains its order. Include
a direct noncontiguous-sibling/nested-child table witness with literal outcomes.

The core entry receives span<const node> and an optional pointer to an owned input
shared_ptr. Null output means audit-only: do not materialize a descriptor. Tables
are internal validated frontend products, never an untrusted public input format.
Projectors preserve one parent per nonroot, all nodes reachable in source order,
in-range indexes, acyclic links, matching child counts and source lifetimes.
Internal impossible states are programming errors, not an alternate user-facing
JSON policy. Tests must cover topology projection and bounded row count.

The legacy front end owns both DOM and the completed raw event vector throughout
the synchronous core call; do not move/mutate viewed strings or return views from
a destroyed local RawNumberHandler. Own document remains alive through the call.

The own interface is exactly one document-taking function:
decodeOwnNativeEllipseInput(const formats::detail::OwnJsonDocument&)
returning NativeEllipseInputResult. It borrows only for the synchronous call;
default empty document returns InvalidJson `/` and no input. Do not add byte/audit
overloads without a real consumer. A caller first invokes readOwnJson, explicitly
maps InvalidJson/ResourceLimit+owned path, and calls own admission only for
Parsed+nonnull. Tests exercise this composition with a local test helper; enum
mapping is explicit, never ordinal casting. No production route is switched.

## Gate order and exact behavior

Required legacy observable gate order: byte limit -> literal NUL/empty -> complete pinned iterative encoding-
validating DOM parse -> current BFS resource/duplicates -> raw-number SAX plus
complete preorder event-kind alignment -> exact number normalization -> grammar ->
optional descriptor publication. Keep source-order duplicate/count precedence,
depth/count limits, malformed-suffix precedence and old empty-ancestor path quirk.
Do not insert E as prefilter, copy binary eligibility or normalize Unicode.
Current code normalizes numeric events during kind alignment before its final
event-count check. F factors normalization into the core after successful full
alignment; do not mislabel this internal factoring as byte-identical sequencing.
Raw parse/kind/count failures must still return InvalidJson `/`, not become the
internal-table programming-error category. Schema still follows all normalization.

Own path: E syntax/scalar/resource result -> projection -> same exact grammar ->
descriptor publication. E failure wins before schema and preserves exact owned
path, including empty ancestor segments. Lexical overflowing numbers can reach
the grammar; lone escaped low surrogates cannot. These are explicit private-policy
distinctions, not a transparent legacy replacement. No resource revalidation with
new precedence inside the grammar core.

Extract, do not rewrite, first-party decimal functions. Normalize all raw number
nodes before schema validation. Exact signed-zero canonicalization, arbitrary
decimal exponent strings, bounded integer extraction, range/strict-positive tests
and keyframe continuity stay exact. No floating conversion or exponent expansion.
Object validation still checks unknown fields in source order, missing fields in
required-list order, then optional nm. Root v precedes fr; all grammar short-circuit
order and positional arrays remain as implemented. Names remain decoded-byte
limited256, reject embedded zero, retain absent versus present empty names.

Core paths use the existing schema helper convention. Supported grammar names
are nonempty; unknown empty members reject immediately without descending. E's
resource diagnostics are already returned before this core and must not be
reformatted. Preserve legacy malformed scalar bytes wherever its route accepts.

Accepted descriptors own every retained name/coefficient/exponent/array/variant.
Overwrite input bytes and destroy document, events, tables and frontend handles;
every descriptor field remains equal to independent expected data. Rejection
publishes no descriptor. No global normalization cache; concurrent calls isolate
their state. Ordinary allocation exceptions retain existing semantics.

## Verification

Task1: compile a core stub and a direct complete-valid-table/expected-descriptor
test; observe successful-build functional RED, then extract the shared core and
connect only legacy front end. Pin literal legacy code AND full path bytes before
refactor; retain all old tests. Characterization's109 observations identify cases,
not a substitute for complete descriptor expectations or an exhaustive oracle.
Review exact old frontend flags/order and unique compiled grammar, not equality
tests alone. No artificial production regression solely to obtain a red test.

Task2: own adapter compiling rejection stub -> real accepted-document RED -> own
projection -> GREEN. None and Telegram direct tests assert full independently
literal baseline and static descriptors, all optional names, variable fields,
tiny/huge exponents, structural bounds, signed zero, source lifetime, two independent
threads versus serial literals, default empty document and no failure publication.
TGS uses only existing Formats decode and committed fixtures; no downloaded corpus.

Telegram-only supplement compares complete old/own code/path/publication and every
descriptor field on common inputs. Both paths share grammar, so this is adapter
consistency evidence, not independent grammar truth. Deliberate differences use
literal expected outcomes for each side and are counted separately:

| Mutation / input | Old | Own composed path |
| --- | --- | --- |
| baseline fr=0e309 | InvalidJson `/` | UnsupportedValue `/fr` |
| baseline ip=0e999999999999999999999999 | InvalidJson `/` | Accepted baseline descriptor |
| baseline fr=1 followed by309 zeros then e-309 | InvalidJson `/` | Accepted rate+1e0, other fields baseline |
| baseline fr=1.7976931348623158e308 | InvalidJson `/` | UnsupportedValue `/fr` |
| baseline fr=1.79769313486231580e308 | UnsupportedValue `/fr` | Same |
| baseline fr=1e-9999 | Accepted exact+1e-9999 | Same full descriptor |
| baseline layer nm=escaped DC00 | Accepted bytes ED B0 80 | InvalidJson `/` |
| {"x":1e309} | InvalidJson `/` | UnsupportedField `/x` |
| {"x":1,"x":2,"y":310-digit compensated token} | InvalidJson `/` | InvalidJson `/x` |
| {"":{"a":1,"a":2}} | InvalidJson `/a` | InvalidJson `//a` |

Pin combined grammar failures and source/required order, names with slash/tilde/
NUL, own/legacy depth/count empty ancestors, exact byte limits, malformed suffixes
and numeric differences in unknown branches. No generated mismatch whitelist.

After independent task reviews: fresh full MSVC none, Telegram, Win32 preview
with existing capture/WARP/device-recreation tests; Samsung configure boundary;
vendor/TGS integrity. Actual include/link evidence proves own core/adapter has no
vendor-header dependency and none link closure has no vendor; Telegram Runtime
intentionally remains reference-linked and old source stays gated. Retained C/D tests guard existing
binding/certificate/stream behavior; they do not certify an own model. Independent
whole-stage review before acceptance and ordinary push with remote guard.

## Global constraints

Direct main, small scoped commits, ordinary push after verification/review; no
force, rebase, history rewrite. One product writer. Preserve raw/SDD. No vendor,
dependency/license/notice/fixture/golden edits, install-policy changes, public
fallback, Player, Direct2D ownership, ANGLE/backend coverage or Runtime.cpp route
changes. No dependency installs, Windows/settings/security changes, automation,
external publication or UI actions. All new interfaces stay private/not installed.

F does not claim full Lottie support, reference-free playback, native GPU rendering,
universal legacy scalar compatibility, TSan, zero allocations or measured speedup.
Next model/ID/resource construction and certificate decoupling remain separate
designs, followed by native timeline/pixels and isolated static Avelabs acceptance.


## Self-review and approval — 2026-09-24

Controller reviewed all779 lines of the current legacy implementation and both
private headers. The independent read-only draft check found no architecture
contradiction and identified token-owner lifetime, noncontiguous sibling indexes,
and normalization/alignment factoring clarifications; all are included above.
Scope is one shared core and one own adapter, not a parser framework or loader.
Limits, old gate order, ownership, explicit policy differences, private linkage,
independent literal tests and final host destination are concrete. No placeholder
or unsupported compatibility/performance claim remains. Controller approves this
written spec under delegated authority and proceeds to writing-plans before code.
