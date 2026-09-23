# Part25D Task 1 — Telegram source ownership and synchronized binding refresh

Implementation base: `15f347a7b065fe32123fc8fb01fe964d40db2f5f`, on the user's chosen main checkout. This report accompanies commit `fix: synchronize retained Telegram source bindings`; no push was performed by the implementer. Controller-owned STATE/spec/plan edits are outside this commit. No helpers or reviewers were spawned.

## Implementation

Added the exact private interfaces requested by the approved design: `rlottie::AveMotionAnimationAccess::model(const Animation&)`, `fromModel(const std::shared_ptr<LOTModel>&)`, and the shared-source overload of `buildTelegramParsedModel`. The factory rejects null model/root and uses the existing Animation constructor/init. A source lease retains only authored data; each factory call owns a separate composition/evaluator. The JSON/key builder remains available with its original errors.

LOTModel now owns a binding mutex and initially-zero atomic uint64 epoch. Animation init locks only composition construction and epoch capture. Every raw source-ID constructor read is covered. At recording sample entry an acquire epoch load selects either the existing reset with local bindings only, or a mutex-protected reset that refreshes bindings and captures the current epoch. Evaluation/tree building occurs after unlocking. Four typed path and four typed paint constructors pass their existing data pointer to a borrowed const LOTData owner in the base; groups, hidden layers, compositions and repeaters propagate refresh. Sequential render-ID assignment, geometry/paint algorithms and multi-path validity/count semantics are untouched.

Both builder overloads and the property oracle route Extractor::build through one locked helper. Its BindingPublication guard is declared after the lock and release-increments the epoch on success, failure and exception unwinding before unlock. Extractor construction precedes the helper but only initializes descriptor-owned output; it does not read/write source bindings. Partial failed-extraction stamping remains visible; no partial final model is published. No exception translation or public fallback policy changed.

Raw-ID audit against the base found exactly eight reads in the four path/four paint constructors and three writes (layer/group/data) in Extractor. Current raw reads occur only in the two base constructors and the two `refreshBindings` branches. Local path/paint copies feed LOTDrawable and C publication; the bridge consumes those published fields. Captured command output: `out/part25d-bindings/self-review.txt` and `audit-and-diff.txt`. Unchanged-epoch source review confirms no mutex acquisition or raw LOTData binding read on that branch. Mask/clip/drawable resets have no raw bindings and retain their signatures. Null/solid/image layers do not acquire invented authored path IDs.

Runtime.cpp, ReferenceRuntime.cpp, Samsung, ordinary algorithms, comparator/goldens, corpus, licenses/notices, Direct2D and Player were not edited. No dependencies or system/global settings changed. Runtime adoption/cache eviction integration remains the next bounded task.

## TDD and verification evidence

Used the TDD skill and its writing-good-tests reference; the systematic-debugging skill was used for a replay-harness newline mismatch, and verification-before-completion for final checks. Read the task brief, complete approved spec and `out/part25d-design/source-binding-options.md`; did not read the whole implementation plan or repeat Part25C work.

All raw commands/output are under the new ignored `out/part25d-bindings/` directory. Reproduce commands from the repository root after:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
```

The evidence runner `out/part25d-bindings/run.ps1` logs that full environment command, revision, supplied CMake/CTest commands, output and exit code. Commands below are the actual underlying commands.

1. **Functional RED, before production edits** (`red.txt`):

   ```bat
   cmake --preset windows-msvc-telegram-debug
   cmake --build --preset windows-msvc-telegram-debug --target avemotion_source_binding_tests --parallel 4
   ctest --preset windows-msvc-telegram-debug -R avemotion.runtime.source_bindings --output-on-failure -V
   ```

   The real recording API loaded a cold whitespace-distinct cached fixture, copied a visible single-path scene, called the existing JSON/key parsed builder, then compared to a fresh ordinary scene with asserted live authored IDs. It failed functionally: `full scene mismatch: scene.drawItems[0].sourcePathNode differs`. CTest 0/1, EXIT=8. No comparator/ID exclusions or relaxed tolerance. The source getter/factory, owner matrix, failure/publication, property and concurrency contract bodies were added before implementing their new APIs; this compile-dependent supplementation is not presented as the functional RED.

2. **Focused iteration** (`green-attempt-1.txt`): production compiled, but supplementary test code failed compilation due to string-literal pointer addition and using `AssetHandle.value` instead of its real index/generation fields. Corrected those test mistakes; this is not functional RED evidence. `green-attempt-2.txt` then passed 2/2 (22.70 s).

3. **Final focused GREEN** (`green-final-focused.txt`):

   ```bat
   cmake --build --preset windows-msvc-telegram-debug --target avemotion_source_binding_tests avemotion_recording_lifecycle_tests --parallel 4
   ctest --preset windows-msvc-telegram-debug -R "avemotion.runtime.(source_bindings|recording_lifecycle)" --output-on-failure -V
   ```

   2/2 passed, EXIT=0, 26.78 s. Source binding test 0.19 s; unchanged recording lifecycle test 26.58 s. Added CTest name `avemotion.runtime.source_bindings`, Telegram only. The new test covers:

   - Cold IDs, later preparation via JSON/key and shared handle, exact full copied-scene parity, unchanged old copies and retained root identity.
   - Null source/root rejection, exact shared pointer identity, lease lifetime, metadata/count parity through the full scene comparator, and factory/load ordinary CPU pixel equality.
   - All 16 rect/ellipse/shape/polystar × solid/gradient fill/stroke combinations with live applicable authored IDs, plus 16 multi-path cases with deliberately invalid path ID, valid paint ID and count 2. Preparation while hidden, then visible/repeated samples.
   - Hidden repeater/nested repeater/outer trim histories with and without a pre-preparation sample; full scene comparison after each seek/repeat.
   - Actual duplicate-layer failure: change child `ind:2` to `ind:1` in `recording-negative-active-control.json`, frame 2. The original error `parsed composition contains duplicate layer IDs` remains, the model is null, epoch changes, fresh ordinary scene has live partially stamped bindings, and retained scene matches every field.
   - Property-oracle extraction updates the source epoch and refreshes a previously cold recording session.
   - Four host threads: two distinct descriptors perform 40 extractions each on one source while two workers construct ordinary/recording sessions and sample two original cold retained sessions. No Animation is shared concurrently. After joining, descriptor-specific handles/debug names/final model ownership are checked and original retained-session snapshots match a valid fresh ordinary oracle.

4. **Full Telegram gate** (`full-telegram.txt`):

   ```bat
   cmake --preset windows-msvc-telegram-debug
   cmake --build --preset windows-msvc-telegram-debug --parallel 4
   ctest --preset windows-msvc-telegram-debug --parallel 4 --no-tests=error --output-on-failure
   ```

   Full configure/build and all **64/64 tests passed**, EXIT=0, 56.00 s. Source bindings passed in 0.24 s and recording lifecycle in 45.36 s under parallel suite load. Scene/model/parsed-model/evaluation/plan/source-geometry goldens, vendor/corpus checks and Git protected-byte roundtrip all passed. No test exclusion or whitelist was used.

## Provenance and self-review

New numbered patch: `patches/telegram/0007-avemotion-source-bindings.patch`. Generated by Git from the exact base and staged vendor changes, with binary/full-index output. Empty context-line space prefixes are removed, matching existing patches and avoiding diff-check trailing-whitespace findings; source payloads are unchanged. `out/part25d-bindings/provenance.ps1` archives the base vendor tree and its `.gitattributes`, creates a disposable nested replay repository, checks/applies the patch and SHA-256 compares every changed/new vendor file. The six-file byte match passes in `patch-reapply-final.txt`, and again for the formatted final patch in `patch-reapply-checked.txt` (base explicitly logged). `vendor-verify.txt` confirms both vendors and corpus fingerprints pass the canonical verifier.

The initial replay (`patch-reapply.txt`) intentionally remains as failure evidence: its mini-repository omitted `.gitattributes`, allowing installed core.autocrlf to convert the patched header. `patch-diagnosis.txt` proves 16,529 live versus 17,029 replay characters, exactly equal after CRLF normalization; live `text` was unset, replay unspecified. Including the base attributes fixes the harness without product-byte or Git-setting changes. The corrected replay is in a new directory; no destructive cleanup occurred.

Telegram final canonical source fingerprint: `f3ed8fa6159db389efb4b156bb6d741c1a9b0d3893cff1bab8de9207a8eddc2a`, 277 files, 13,286,789 bytes. UPSTREAM.json updates only the Telegram local patch list and source fingerprint/count/bytes. Original Telegram/Samsung commit, archive, archive hashes, archive counts/bytes and license texts are preserved. Patch README and THIRD_PARTY describe the new private seam and its separate Runtime adoption stage.

Self-review checked lock scope/order, release-before-unlock guard destruction, error preservation, constructor coverage, all reset propagation, no sequential-ID reassignment, borrowed pointer lifetime, no hidden per-frame composition reconstruction and test oracle independence. `audit-and-diff.txt` contains clean product diff-check runs and the unchanged scope check. `staged-check.txt` preserves the initial generated patch's blank-context whitespace findings; `staged-check-final.txt` records the corrected final staged file names, diff checks and unchanged scope. `commit.txt` records the resulting scoped commit.

Concerns/limits: functional concurrency stress is **not TSan** and no race-detector claim is made. Exception publication follows RAII/source review; no forced allocation-failure injection was run. This task's platform run is MSVC Debug; Release/cross-compiler/platform integration and source-lease cache-eviction behavior inside Runtime remain later-stage obligations. Existing upstream MSVC D9025/C4251/C4530 warnings persist; the build is not warning-free. No known functional defect or design conflict remains in this task.
