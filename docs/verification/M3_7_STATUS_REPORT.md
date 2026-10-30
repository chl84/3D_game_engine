# Status report — 2026-10-30 — M3.7

Previous verification below applies to7c24116 only. The latest owner clarification
requires independent Claude Opus re-review of all three fixes, especially queued
deadlines, plus durable live JSONL. M3.7 is not approved. New journal changes are
being verified and will be included in that re-review before a new owner report.

**Delivered:** A local JSON-RPC server and CLI client for persistent in-memory
scene authoring, inspection and screenshots. Live and offline authoring use the
same CommandBus and handler registration. The current owner-selected reviewer is Claude Opus5.5 high, documented in
AI_AGENT_GUIDE.md. The earlier Astra review remains historical evidence.
M3.6 is owner-approved; M3.8, merge and tag remain unauthorized.

**Verification source:** `7c241168583e5cd1b37c6b14aa60fd959819753d`.
The source consists of the M3.7 implementation, the reviewed corrections, and
a Windows-only ACL probe environment correction. Final documentation may have
a later commit; the tested source SHA is recorded separately.

| Function | Status | Evidence |
|---|---|---|
| Shared CLI/live command implementation | Automatically tested | Shared register_project_commands and CommandBus; live/CLI schema equality and actual command-log entries checked by engine_live_api |
| Loopback-only listener and authentication | Automatically tested | Alternate 127.0.0.2 destination rejected; missing, wrong-length and wrong same-length credentials give exact generic Unauthorized, id:null and no request details |
| Parser validation and bounds | Automatically tested | 64 KiB payload, depth 32, strict UTF-8, duplicate members, malformed JSON, protocol/version/params/ID cases; unit and integration tests |
| Connection cap, deadlines and disconnects | Automatically tested | Eight accepted clients; absolute receive/send deadlines; idle fairness, trickled input, incomplete EOF and complete-request RST; server remains responsive |
| Deadline after a blocking handler | Automatically tested | engine_local_socket_deadline: a pending frame completed after expiry never dispatches; an already accepted handler may finish; removing the new checks fails the test |
| Private discovery and token rotation | Automatically tested | CSPRNG 32 bytes; owner-only file from creation, permission/owner readback before writing; restart rotates the credential; shutdown removes discovery and lock |
| Client path sandbox | Automatically tested | Import/save/load/screenshot relative, absolute and NUL escapes rejected; symlink escapes checked where available; canonical internal-path aliases rejected; live reimport cannot read an external original |
| Exclusive project writer lock | Automatically tested | Offline entity/import/screenshot, second live instance and active force-unlock return E_CONFLICT; offline writes resume after shutdown |
| Live-only asset workflow | Automatically tested | Import/list/inspect/validate/place/save/load/reimport/screenshot through RPC, schema-validated responses, stable IDs and exact optional PNG output |
| Shared screenshot and history | Automatically tested | Existing offline limits/error contract preserved; scene load clears stale undo/redo; delete/undo restores assets and both light components |
| Bounded command log | Automatically tested | Separate 4,096-entry and 16 MiB admission tests; last completed entry retained; paged log, ping and shutdown still available |
| Local Clang and ASan/UBSan | Automatically tested | Corrected source: 24/24 for each; leak detection enabled, documented local Mesa retention only, no LSAN_OPTIONS or suppression file |
| Physical Iris Xe live flow | Manually verified | Full live integration script passed; see captured local evidence |
| Public clean-clone acceptance | Automatically tested | Exact 7c24116 clone: 24/24, M1 acceptance passed and M1 clean-clone acceptance passed |
| Reference-major static tools | Automatically tested | clang-format18 passes all tracked first-party C++; clang-tidy18 passes all 17 changed C++ translation units with compatible Ubuntu GCC14 headers; incompatible native-header attempt is recorded separately |
| Rendering regression and color golden | Automatically tested | Linux and Windows lavapipe: fixed PBR/color PNGs passed, Blender tangent image difference 0, emissive patch 117 as sRGB |
| Three permanent fuzz targets | Automatically tested in reference CI | Same existing job: GLB 5,553, image 7,794 and RPC 64,402 executions, 31s each; no sanitizer findings |
| Reference cross-platform CI | Automatically tested | [41093710140](https://github.com/chl84/3D_game_engine/actions/runs/41093710140), exact 7c24116: Linux GCC/Clang/ASan 24/24 and Windows 18/18; Fuzz and Quality passed; all six jobs completed/success |
| Independent review and corrections | Manually verified | GPT-6 Astra high reviewed 6e3fb73; MSVC iterator, formatter18 and queued-deadline findings corrected; [review/closure record](M3_7_REVIEW_ROUND1.md) |

[CI evidence](M3_7_CI_EVIDENCE.txt) and
[local evidence](M3_7_LOCAL_EVIDENCE.txt) contain commands, captured results,
negative controls and clean-clone proof. Wire contracts are
[request](../../schemas/jsonrpc-request@1.schema.json) and
[response](../../schemas/jsonrpc-response@1.schema.json); method results use
the existing command-result@1 envelope. ADR-0013 documents the complete profile.

## Bounds and behavior

- One LF-delimited UTF-8 JSON object per connection; maximum 64 KiB request,
  JSON depth 32 and 4 MiB reply. Eight accepted connections and bounded backlog.
- Receive deadline is absolute from accept and is not extended by fragments.
  Send deadline starts after dispatch. Default 2,000 ms; configurable 50..30,000 ms.
  Deadlines are rechecked after a preceding synchronous handler and immediately
  before a pending frame dispatches.
- Live screenshots are capped at 512×512 and 16 frames. The existing offline
  limits remain 8,192 pixels per dimension and 1,000 frames.
- Every request needs the session credential. Invalid credentials reveal no
  method, protocol, project or request details. Startup stdout contains no token.
- The project writer lock covers the entire session and discovery cleanup.
  Accepted commands are not canceled or rolled back on disconnect/client timeout.
- Scene.save/load and history operations are shared commands. Save/load use
  sandboxed canonical .scene.json targets outside control/asset directories;
  load clears stale history. No parallel implementation of authoring commands.
- Session admission stops authoring after 4,096 logged entries or16MiB serialized
  arguments/results, checked before the next dispatch. The last completed record
  is preserved. Log inspection is paged up to 64 entries and does not log itself.

**Not verified / limitations:** Physical Windows presentation is not verified.
No new threads were introduced; TSan was not rerun. Deadlines bound I/O rather than asynchronous CPU/GPU command
cancellation. JSON-RPC batch arrays and positional params are outside the M3
profile. The in-memory log is bounded and durable JSONL is now written continuously;
new verification/re-review is pending. Scene persistence still requires explicit save. Abrupt server
termination can leave a stale lock/discovery requiring the existing recovery
path. Local Mesa retention does not test unload/reload behavior.

**Known defects:** No unresolved confirmed M3.7 source defect after the three
review corrections. The diagnosed local Mesa RTASM unload metadata leak and its opt-in retention policy are unchanged;
ordinary local unload is not claimed clean. No suppression has been reintroduced.

**Deviations from plan:** No new third-party dependency or breaking public
format/API. Platform uses native sockets/private-file permissions/CSPRNG/process
status and Windows system ws2_32/advapi32/bcrypt libraries. Live runs in the
foreground and renders the in-memory scene headlessly through the shared renderer.
Related fixes reject force-unlock of a running owner, lock offline screenshot
writers, reject NUL paths centrally and restore shared light snapshots on undo.
Existing tests and sanitizer/quality policies were not weakened. Windows ACL
assertions are unchanged; only the child PowerShell module environment is reset.

**Decisions requiring approval:** Owner assessment of M3.7. No additional A.6
exception is requested. Merge, tag and M3.8 remain separate owner decisions.

**Next step:** Complete journal verification and mandatory Claude Opus re-review,
handle findings and only then deliver the updated consolidated A.5 report.
No merge, tag or M3.8 is authorized.

## Durable JSONL addition and mandatory re-review (pending)

The owner requested live persistence under .engine/ and restored Claude Opus
as reviewer. The parent implements this in M3.7 before combined re-review of
all three prior fixes plus journaling. New contract: journalVersion1 JSONL,
private per-session file, synchronized started intent before dispatch, actual
completed CommandLog results before acknowledgement. Both successful and failed
commands are preserved. Journal failure stops dispatch; failure after execution
returns E_IO with an uncertain-outcome hint, never a success acknowledgement.

New engine_live_journal covers hard termination without destructors/shutdown,
complete acknowledged JSONL before the crash, restart retention, failed commands,
credential exclusion and append-failure prevention of a subsequent mutation.
Platform unit tests cover synchronized append, missing/directory/symlink/hardlink
targets and POSIX mode rejection. Local Clang and final-source ASan/UBSan both pass25/25. Expected reference totals are Linux25/25 and
Windows19/19. New Windows/private journal runtime verification and Claude
re-review remain pending. [Journal evidence](M3_7_JOURNAL_EVIDENCE.txt) also records
a manual post-execution fsync fault: the actual scene write occurs, the RPC
returns E_IO and the session stops. No success or atomic rollback is claimed.

This supersedes the old in-memory-only limitation; scene state is still saved
explicitly. Started without completed is uncertain; retain complete earlier
JSONL lines and treat an incomplete final line as incomplete. Do not auto-replay
an uncertain command. Files are retained per session, without automatic total
disk retention/archival. This is process-crash durability, not a power-loss
guarantee or an atomic transaction with scene/asset writes.

## Claude review findings and follow-up (pending)

Claude independently verified all three original corrections and the journal
design, but requires F1 serialization fail-closed correction. F2–F4 strengthen
the mode probe, automate native fsync faults and expose terminal failure on
stderr/exit6, including notifications. The parent addresses all four together;
new-source CI and another Claude check are required before final reporting.
[Claude findings](M3_7_CLAUDE_REVIEW_ROUND1.md). New expected totals are Linux
GCC/Clang26/26, ASan25/25 and Windows19/19; the additional real fsync fault
interposer runs only in Linux non-sanitized builds.

[Follow-up local evidence](M3_7_CLAUDE_FIX_EVIDENCE.txt) records the new tests
and discriminating source mutations. Independent Claude re-check and new
reference CI remain required before final reporting.
