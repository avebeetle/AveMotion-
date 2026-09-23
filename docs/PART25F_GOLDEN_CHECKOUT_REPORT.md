# Part25F — exact golden checkout preservation

Bounded maintenance follow-up after the accepted lean-module stage; this is
not native scene emission. Product commit `fc7b53c3c5954369ee80f8a0dcfbef0253b01dbf`
adds `/tests/golden/** -text` and three real-Git regression cases for LF TSV,
LF TXT and CRLF TXT. No golden, comparator, runtime, vendor or global setting
was changed. The rule preserves committed bytes rather than normalizing them.

The new test first failed on actual LF-to-CRLF conversion, then passed with
both `core.autocrlf=true` and `false`. All 12 current golden files and an isolated
true-mode checkout match their pre-change BASE Git blobs byte for byte. No
separate pre-edit worktree SHA inventory file was retained; the reproducible
proof is BASE/HEAD blob equality plus current/isolated-checkout raw equality.

Fresh tests: Telegram Debug 68/68; focused Telegram 15/15; focused preview 8/8;
Samsung integrity 4/4 and no-reference integrity 3/3. Exact generated-output
comparisons passed for six Telegram and three preview files. The focused preview
selection covered TGS corpus integrity, not vendor.verify/manifest. The controller
separately ran the complete all-variant vendor/corpus verifier and real Git test,
both exit 0. Earlier full graphics gates remain documented in the Part25E report.

Independent complete-change review approved spec compliance and code quality
with no Critical or Important findings. Its Minor report-coverage wording issue
was corrected without a product change. Brief, report, review and raw evidence:
`out/part25f-golden-checkout/`, including `controller-final-verification.txt`.

The resulting cloud outcome is not yet observed. Linux raw fingerprint
differences remain separate and unresolved; no test or golden was weakened.
The user requested a wrap-up because of usage limits. Automation `avemotion-6`
was first paused, then DELETED at the user's explicit request to cancel the
15-minute schedule. No new implementation stage is running. Continue only on a new request,
starting from `docs/superpowers/STATE.md` and inspecting the post-push CI result.
