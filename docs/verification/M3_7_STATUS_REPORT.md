# Status report — 2026-10-30 — M3.7

Final verification is being assembled. Do not deliver the owner report until
all six reference jobs have succeeded. The independent review is complete and
its three findings have been corrected; no second independent verdict is claimed.

**Delivered:** A local JSON-RPC server and CLI client for persistent in-memory
scene authoring, inspection and screenshots. Live and offline authoring use the
same CommandBus and handler registration. The owner-selected reviewer is
GPT-6 Astra with high reasoning effort, documented in AI_AGENT_GUIDE.md.
M3.6 is owner-approved; M3.8, merge and tag remain unauthorized.

**Verification source:** `7c241168583e5cd1b37c6b14aa60fd959819753d`.
The source consists of the M3.7 implementation, the reviewed corrections, and
a Windows-only ACL probe environment correction. Final documentation may have
a later commit; the tested source SHA is recorded separately.

| Function | Status | Evidence |
|---|---|---|
| Shared CLI/live command implementation | Automatically tested | Shared register_project_commands and CommandBus; live/CLI schema equality and actual command-log entries checked by engine_live_api |
| Loopback-only listener and authentication | Automatically tested | Alternate 127.0.0.2 destination rejected; missing, wrong-length and wrong same-length credentials give exact generic Unauthorized, id:null and no request details |
| Parser validation and bounds | Automatically tested | 64 KiB payload, depth32, strict UTF-8, duplicate members, malformed JSON, protocol/version/params/ID cases; unit and integration tests |
| Connection cap, deadlines and disconnects | Automatically tested | Eight accepted clients; absolute receive/send deadlines; idle fairness, trickled input, incomplete EOF and complete-request RST; server remains responsive |
| Deadline after a blocking handler | Automatically tested | engine_local_socket_deadline: a pending frame completed after expiry never dispatches; an already accepted handler may finish; removing the new checks fails the test |
| Private discovery and token rotation | Automatically tested | CSPRNG32bytes; owner-only file from creation, permission/owner readback before writing; restart rotates the credential; shutdown removes discovery and lock |
| Client path sandbox | Automatically tested | Import/save/load/screenshot relative, absolute and NUL escapes rejected; symlink escapes checked where available; canonical internal-path aliases rejected; live reimport cannot read an external original |
| Exclusive project writer lock | Automatically tested | Offline entity/import/screenshot, second live instance and active force-unlock return E_CONFLICT; offline writes resume after shutdown |
| Live-only asset workflow | Automatically tested | Import/list/inspect/validate/place/save/load/reimport/screenshot through RPC, schema-validated responses, stable IDs and exact optional PNG output |
| Shared screenshot and history | Automatically tested | Existing offline limits/error contract preserved; scene load clears stale undo/redo; delete/undo restores assets and both light components |
| Bounded command log | Automatically tested | Separate 4,096-entry and16MiB admission tests; last completed entry retained; paged log, ping and shutdown still available |
| Local Clang and ASan/UBSan | Automatically tested | Corrected source:24/24 for each; leak detection enabled, documented local Mesa retention only, no LSAN_OPTIONS or suppression file |
| Physical Iris Xe live flow | Manually verified | Full live integration script passed; see captured local evidence |
| Public clean-clone acceptance | Automatically tested | Exact7c24116 clone:24/24, M1 acceptance passed and M1 clean-clone acceptance passed |
| Reference-major static tools | Automatically tested | clang-format18 passes all tracked first-party C++; clang-tidy18 passes all17changed C++ translation units with compatible Ubuntu GCC14 headers; incompatible native-header attempt is recorded separately |
| Rendering regression and color golden | Automatically tested | Linux and Windows lavapipe: fixed PBR/color PNGs passed, Blender tangent image difference0, emissive patch117 as sRGB |
| Three permanent fuzz targets | Automatically tested in reference CI | Same existing job: GLB5,553, image7,794 and RPC64,402 executions,31s each; no sanitizer findings |
| Reference cross-platform CI | Automatically tested; overall gate pending | [41093710140](https://github.com/chl84/3D_game_engine/actions/runs/41093710140), exact7c24116: Linux GCC/Clang/ASan24/24 and Windows18/18; Fuzz passed; Quality still running |
| Independent review and corrections | Manually verified | GPT-6 Astra high reviewed6e3fb73; MSVC iterator, formatter18 and queued-deadline findings corrected; [review/closure record](M3_7_REVIEW_ROUND1.md) |

[CI evidence](M3_7_CI_EVIDENCE.txt) and
[local evidence](M3_7_LOCAL_EVIDENCE.txt) contain commands, captured results,
negative controls and clean-clone proof. Wire contracts are
[request](../../schemas/jsonrpc-request@1.schema.json) and
[response](../../schemas/jsonrpc-response@1.schema.json); method results use
the existing command-result@1 envelope. ADR-0013 documents the complete profile.

## Bounds and behavior

- One LF-delimited UTF-8 JSON object per connection; maximum64KiB request,
  JSON depth32 and4MiB reply. Eight accepted connections and bounded backlog.
- Receive deadline is absolute from accept and is not extended by fragments.
  Send deadline starts after dispatch. Default2,000ms; configurable50..30,000ms.
  Deadlines are rechecked after a preceding synchronous handler and immediately
  before a pending frame dispatches.
- Live screenshots are capped at512×512 and16frames. The existing offline
  limits remain8,192pixels per dimension and1,000frames.
- Every request needs the session credential. Invalid credentials reveal no
  method, protocol, project or request details. Startup stdout contains no token.
- The project writer lock covers the entire session and discovery cleanup.
  Accepted commands are not canceled or rolled back on disconnect/client timeout.
- Scene.save/load and history operations are shared commands. Save/load use
  sandboxed canonical .scene.json targets outside control/asset directories;
  load clears stale history. No parallel implementation of authoring commands.
- Session admission stops authoring after4,096logged entries or16MiB serialized
  arguments/results, checked before the next dispatch. The last completed record
  is preserved. Log inspection is paged up to64entries and does not log itself.

**Not verified / limitations:** Physical Windows presentation is not verified. No new threads were introduced;
TSan was not rerun. Deadlines bound I/O rather than asynchronous CPU/GPU command
cancellation. JSON-RPC batch arrays and positional params are outside the M3
profile. The command log is bounded in memory and is not automatically exported
on shutdown; scene persistence requires the explicit save command. Abrupt server
termination can leave a stale lock/discovery requiring the existing recovery
path. Local Mesa retention does not test unload/reload behavior.

**Known defects:** No unresolved confirmed M3.7 source defect after the three
review corrections. The final Quality job is still pending. The diagnosed local
Mesa RTASM unload metadata leak and its opt-in retention policy are unchanged;
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

**Next step:** Finish reference CI, record its actual results and deliver the
consolidated A.5 report. Then await the owner's next-step decision.
