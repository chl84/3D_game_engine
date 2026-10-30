# Status report — 2026-10-30 — M3.7

Working verification record. Do not deliver an owner status report until
independent review is handled and required CI/re-review is complete.

**Delivered:** Authenticated loopback JSON-RPC server/client using the existing
CLI CommandBus and shared handler registration, bounded framing/connections/I/O,
private discovery, project-lock exclusion and a permanent parser fuzz target.
M3.6 is owner-approved. The owner selected GPT-6 Astra high for new reviews.

| Function | Status | Evidence |
|---|---|---|
| Shared CLI/live authoring | Automatically tested locally | Same register_project_commands/CommandBus; schema equality and actual log entries checked |
| Loopback and per-request auth | Automatically tested locally | Alternate127.0.0.2 destination fails; wrong/missing token yields exact generic Unauthorized with no data/echo |
| Parser resource/error handling | Automatically tested locally | 64KiB, depth32, duplicate members/invalid UTF-8/syntax/envelope cases; bounded pure-parser fuzz target |
| Connection lifetime | Automatically tested locally | New queued-deadline regression rejects expiry during a preceding slow handler; Absolute I/O deadlines,8client cap, idle fairness, partial EOF and complete-request RST; server remains responsive |
| Private token discovery | Implemented and POSIX verified | CSPRNG32bytes;0600/current UID; Windows protected current-user-only DACL/owner readback implemented but not yet reference-tested |
| Token rotation and shutdown | Automatically tested locally | Normal shutdown removes discovery/lock; restart rejects old credential and accepts new one |
| Sandbox | Automatically tested locally | Import/save/load/screenshot relative/absolute/NUL escapes rejected; symlinks checked where available; live reimport cannot read external original |
| Lifetime project writer lock | Automatically tested locally | Offline entity/import/screenshot, second live session and active force-unlock return E_CONFLICT; writes resume after shutdown |
| Live asset acceptance | Automatically tested locally | Import/list/inspect/validate/place/save/load/reimport/screenshot entirely through RPC; PNG bytes match optional output; IDs/scene bytes preserved |
| Shared screenshot and history | Automatically tested locally | Existing CLI limits/error contract retained; scene load clears stale undo; delete/undo restores asset and both light components |
| Session log admission | Automatically tested locally |4096-entry and16MiB admission limits exercised separately; last completed record retained; paged log and shutdown remain available |
| Local Clang | Compiled and automatically tested |24/24 on lavapipe, no skips after review corrections |
| Local ASan/UBSan | Automatically tested |24/24 with leak checks and documented Mesa retention, no suppression file |
| Local three-target fuzz | Automatically tested |30s each: GLB4994, image8284, RPC66839runs; no sanitizer findings |
| Windows/reference CI | Not verified for this source yet | Must run new exact-SHA CI; expected corrected Linux24/24 and Windows18/18 |
| Independent review | Completed; correction verification pending | GPT-6 Astra high reviewed6e3fb73 and requested MSVC iterator, formatter18 and queued-deadline fixes; see [round1 record](M3_7_REVIEW_ROUND1.md) |

[Local evidence](M3_7_LOCAL_EVIDENCE.txt). Wire contracts are
[request](../../schemas/jsonrpc-request@1.schema.json) and
[response](../../schemas/jsonrpc-response@1.schema.json); methods return the
existing command-result@1 envelope. ADR-0013 documents the complete profile.

No new third-party dependency, breaking project/scene/asset schema or weakened
existing test is introduced. Platform adds native sockets, private-file
permissions/CSPRNG and process status; the planned control target provides the
bounded parser. Windows uses only system ws2_32/advapi32/bcrypt libraries.

**Not verified / test limitations:** Windows compilation/DACL/socket execution
until new CI completes; physical Windows presentation; TSan (no new threads);
command CPU/GPU cancellation. Deadlines cover receive/send I/O; an accepted
command is not rolled back on disconnect/client timeout. RPC batch arrays and
positional params are outside the documented M3 profile. Session command log
is in memory and bounded, with no automatic save/export on shutdown. Local
retention does not test Mesa unload/reload behavior.

**Known defects:** Existing Mesa RTASM unload metadata leak is diagnosed and
covered by the opt-in local retention policy; ordinary local unload is not
claimed clean. Source importer/renderer budgets and performance limits remain
as documented for M3. Review changes are corrected in source; final correction CI is still pending.

**Deviations from plan:** No new dependency. The CLI launcher is a foreground
live serve command plus live call client; screenshots render the persistent
in-memory scene headlessly. Rendering/PNG code and authoring handlers are shared.
The project lock improvement rejects force-unlock of a running owner and adds
screenshot to offline writers. Shared light snapshot undo restoration is fixed.

**Decisions requiring approval:** None newly proposed within the authorized
M3.7 contract. Owner assessment/merge/tag/M3.8 remain separate and unauthorized.

**Next step:** Verify the corrected source in exact-SHA reference CI, then record
closure and deliver the consolidated A.5 report. No second independent verdict
is claimed for the parent correction.
