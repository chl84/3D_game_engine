# Status report — 2026-10-30 — M3.7

Historical snapshot: reviewer-selection instructions here are superseded by the
owner correction on 2026-10-31. Active reviewer: GPT-6 Astra high, configured in
[REVIEWER.json](../../REVIEWER.json). Preserve the actual review attribution below.


**Final status:** Owner-approved on2026-10-30. Claude Opus5.5 high independently
approves the corrected source with no blocking or required findings. All six
reference CI jobs completed/success on the exact reviewed source. The mandatory
re-review, durable JSONL and follow-up findings are complete. M3.8 is unstarted.

**Delivered:** Local JSON-RPC server/client using the same authoring CommandBus
and handlers as offline CLI, bounded authenticated loopback transport, sandboxed
paths, exclusive project locking and durable per-session JSONL command history.
The latest owner decision restores Claude Opus as reviewer. Reviewer selection
belongs to the owner; historical Astra attribution is preserved.

**Verified source:** `d0797e1b361f4f4379b69d33ebc523ab88e93035`.
[Reference CI41108912088](https://github.com/chl84/3D_game_engine/actions/runs/41108912088).
Any subsequent documentation commit is separate from this tested code SHA.

| Function | Status | Evidence |
|---|---|---|
| Shared CLI/live authoring | Automatically tested | Same register_project_commands and CommandBus; schema equality, actual log entries and live-only import/place/save/load/reimport workflow |
| Loopback/token | Automatically tested |127.0.0.1 only; wrong/missing/wrong same-length tokens return generic Unauthorized without request/project details; token rotation tested |
| Message/parser/connection limits | Automatically tested |64KiB request,4MiB reply, JSON depth32,8clients, absolute receive/send deadlines; malformed/truncated/oversized frames and disconnects rejected |
| Queued deadlines after a slow handler | Automatically tested and independently reviewed | Production transport fixture, expired queued frame never dispatches; removing both backstop checks fails; Claude confirms steady_clock/order |
| Client paths and writer lock | Automatically tested | PathSandbox rejects relative/absolute/NUL/symlink escapes; offline entity/import/screenshot/second live/active unlock conflict; writes resume after session |
| Private token/journal files | Automatically tested | Owner-only creation and native readback; POSIX uid/mode0600, Windows protected owner-only DACL; link/type rejection |
| Continuous durable CommandBus JSONL | Automatically tested | Intent synced before execution; actual successful/failed command results synced before acknowledgement; hard kill/restart preserves old journal |
| Result serialization failure | Automatically tested | Invalid UTF-8 filesystem target through UTF-8 alias returns E_IO, emits constant diagnostic, exits6; no later dispatch |
| Permission guard | Automatically tested | Owner-writable0644 file rejected; removing0600 guard fails the unit test |
| Real synchronization faults | Automatically tested on Linux non-sanitized builds | engine_live_journal_fault: started/completed fsync EIO, notification failure and no-fault control; mutations and no-op preload rejected |
| Error observability | Automatically tested | Journal errors and notifications exit6 with one credential-free stderr line; normal shutdown exits0 |
| PBR/color regressions | Automatically tested | Linux/Windows lavapipe golden tests pass, Blender tangent difference0, emissive sRGB patch117 |
| Linux GCC/Clang reference | Automatically tested |26/26 in each job, including real syscall fault test |
| Linux ASan/UBSan reference | Automatically tested |25/25, detect_leaks=1:abort_on_error=1, halt_on_error=1; no sanitizer/runtime errors |
| Windows MSVC reference | Automatically tested |19/19, including live/DACL/journal/deadline and offscreen golden tests |
| Three permanent fuzz targets | Automatically tested | GLB2,560, image3,708 and RPC47,289 runs,31s each, same existing job |
| Exact-source public clean clone | Automatically tested |26/26, M1 acceptance and M1 clean-clone acceptance passed |
| Physical Iris Xe live flow | Manually verified earlier | Initial full live integration passed; no new physical GPU run is claimed for the journal corrections |
| Quality | Automatically tested | Final reference formatter18/tidy18 with warnings as errors passed; all six CI jobs green |
| Independent Claude re-review | Manually verified | Round1 confirms all three original fixes; round2 approves F1–F4 and corrected journaling, with nine independently rejected mutations |

Permanent evidence:

- [Final-source CI excerpts](M3_7_CLAUDE_CI_EVIDENCE.txt).
- [Claude re-review round1](M3_7_CLAUDE_REVIEW_ROUND1.md) and
  [round2 technical approval](M3_7_CLAUDE_REVIEW_ROUND2.md).
- [Corrections, mutations and exact-source clean clone](M3_7_CLAUDE_FIX_EVIDENCE.txt).
- [Initial durable-journal diagnostics](M3_7_JOURNAL_EVIDENCE.txt) and
  [initial live implementation evidence](M3_7_LOCAL_EVIDENCE.txt).

Earlier7c24116/76ecc7d CI is historical only. The original Astra verdict is in
[the original review record](M3_7_REVIEW_ROUND1.md), with the superseding owner
instruction preserved. Parent-only closure was not accepted as independent
approval; the required Claude re-reviews have now been performed.

## Journal contract and bounds

The ready message identifies commandLogFile under
.engine/live-commands-<session UUID>.jsonl. Files are private from creation,
resolved through PathSandbox and retained across shutdown/crash/restart. Tokens
and raw RPC envelopes are not journal fields. Every CommandBus dispatch gets a
journalVersion1 started record synchronized before execution, followed by the
actual sequence/name/arguments/result as completed, synchronized before reply.
POSIX uses fsync; Windows uses FlushFileBuffers. In-memory admission stops after
4,096entries or16MiB serialized arguments/results, checked before the next
command; prior records remain intact. Inspect log remains paged and available.

All dispatch/accounting/serialization/reply exceptions take the terminal journal
path. A failed intent sync prevents execution. A failed completion sync can occur
after a side effect: return E_IO, never success; stop, drain, print a constant
stderr line and exit6. The fault tests verify both orderings and notifications.
Started without completed is uncertain; do not automatically replay. A partial
last JSONL line is incomplete; keep preceding complete records. New sessions do
not append to old damaged files. Journal persistence is not automatic scene
saving: scene.save@1 remains explicit.

Deadlines default to2s, configurable50..30,000ms, and bound receive/send I/O.
Accepted CPU/GPU commands are not canceled or rolled back by disconnect/timeout.
Live screenshots remain bounded at512×512/16frames; existing offline limits are
unchanged. JSON-RPC batches/positional params are outside the documented profile.

**Not verified / limitations:** Physical Windows presentation; power-loss or
storage-controller durability; cross-file atomicity between scene/asset writes
and journal completion. No new threads were added and TSan was not rerun.
The Linux syscall interposer is intentionally not registered in sanitizer or
Windows builds; none of their existing tests is skipped or disabled. Total
archival/retention across session journal files is not automatic. Screenshot is
not a CommandBus command and is not journaled. Oversized replies may be dropped
after the completion is already journaled. Windows readers denying write sharing
can terminate the session fail-closed. Failed startup can leave an empty journal.

**Known defects:** No unresolved required M3.7 finding after Claude round2.
A pre-existing asset.import case with a non-UTF-8 filesystem source can leave an
asset directory while returning a validation error; the journal records that
failure. It is not introduced or fixed by this change and is recorded for owner
assessment during hardening. Local plain ASan GPU runs still show the diagnosed
Mesa unload leak; the documented local retention preset and unsuppressed
reference ASan are green. No suppression has been reintroduced. No leak-clean
claim is made for ordinary local Mesa unload or the fuzz job's existing policy.

**Deviations from plan:** No new external dependency, breaking project/scene/asset
format or command schema. Native system libraries only. Related fixes reject
active force-unlock, lock offline screenshot writers, reject NUL paths centrally
and restore shared light snapshots on undo. Durable JSONL was implemented in
M3.7 at the owner's request. The syscall interposer is test-only. Existing tests
and sanitizer/quality policies were not weakened.

**Decisions requiring approval:** None newly proposed; M3.7 is owner-approved.
Reviewer changes remain exclusively the owner's decision. Merge, tag and M3.8
require separate owner authorization.

**Next step:** Await owner instruction for the next substep. No merge, tag or
M3.8 work is started by this approval record.
