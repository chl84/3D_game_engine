# ADR-0013: Local live API transport

- **Status:** Accepted
- **Date:** 2026-10-30

## Context

M3 adds a live mode that exposes the same commands as the CLI, plus inspection
and screenshot, to local tools and AI agents. The owner approved loopback TCP
with token authentication and required the token file to have restricted
permissions on Linux and Windows. No new dependency should be added.

## Decision

Use JSON-RPC 2.0 over a loopback-only TCP socket.

- Bind explicitly to `127.0.0.1`; never `0.0.0.0` or a public interface. Use an
  ephemeral port.
- Write `.engine/live.json` atomically with:
  `{ "protocolVersion": 1, "port": N, "token": "...", "projectRoot": "..." }`.
- Restrict the token file to the current user:
  - POSIX: mode `0600`.
  - Windows: a DACL containing only the current user SID; no inherited
    `Users`/`Everyone` access.
- If the permissions cannot be enforced, the server refuses to start.
- Every request must carry the token. Reject missing/incorrect tokens and
  non-loopback peers.
- Methods:
  - `command`: the existing command envelope and command bus.
  - `inspect`: project, scene and asset inspection.
  - `screenshot`: render and return PNG data; the default is base64, with an
    optional sandboxed output path.
  - `ping`.
  - `shutdown`.
- Enforce a maximum message size, request timeout, protocol-version check and
  structured JSON-RPC errors. Command results use the same
  `command-result@1` payloads as the CLI.
- The server is single-project and local. There is no remote listener and no
  TLS in M3.

The existing nlohmann/json and platform socket/atomic-file code are used. No
new dependency is added.

## Consequences

- Local tools and AI agents can drive the same command surface as the CLI.
- The token file is a local secret and must never be committed; `.engine/` is
  already ignored.
- Windows DACL handling and POSIX permissions need platform tests.
- Long-running live sessions must handle client disconnects, malformed
  messages and shutdown cleanly.
- A future remote or multi-user API would need a new ADR covering transport,
  authentication and authorization.

## Alternatives considered

- **Unix domain sockets on Linux and named pipes on Windows:** good local
  isolation, but more platform-specific code and two implementations to test.
  Not chosen for M3.
- **HTTP without TLS:** a larger protocol surface and no benefit over JSON-RPC
  on loopback.
- **Named shared memory:** not a good fit for command/result envelopes and
  streaming screenshots.

## M3.7 implementation and owner requirements (2026-10-30)

The owner approved M3.6 and authorized M3.7 with mandatory negative/security
acceptance tests. No new third-party dependency is introduced. Native TCP and
process/private-file code lives in platform; the planned control target provides
only the bounded parser and encoding. Windows links system ws2_32, advapi32 and
bcrypt. CLI/live command handlers are registered through the same
register_project_commands function and actual CommandBus; there is no second
implementation of scene/asset authoring commands.

### Wire profile and bounds

- One UTF-8 JSON object followed by LF per TCP connection; CRLF is accepted.
  Connections close after the reply. Named object params are supported; JSON-RPC
  batches are not part of this M3 profile. Valid authenticated notifications
  execute without a reply. Incomplete EOF, excess bytes/frames, size violations,
  excess connections and I/O deadlines close without dispatching partial data.
- Required root fields: jsonrpc="2.0", protocolVersion=1, token, method; params
  defaults to {}, id is integer/string/null (omitted for notifications).
- 64 KiB request payload, JSON depth32, duplicate object members rejected,
  4 MiB reply payload, eight accepted clients and a bounded backlog of eight.
- Receive deadline is absolute from accept, not refreshed by partial bytes.
  Send deadline starts after dispatch; both default to2000ms, configurable
  through --io-timeout-ms in50..30000. Execution is sequential; accepted
  authoring commands are not canceled or rolled back by client disconnect or
  client-side timeout. These are I/O deadlines, not unsafe asynchronous
  cancellation of filesystem/GPU work.
- Method dispatch/parameter/version details are withheld until authentication.
  Wrong/missing tokens receive only -32001 Unauthorized, id:null, with no data
  or echoed request. Syntax errors use -32700 without parser exception text.
- CSPRNG token:32OS-random bytes encoded as64hex characters. The private file
  and temporary file are restricted from creation, with permissions/DACL read
  back before credential bytes are written. Startup fails closed on failure.
- No token appears in startup stdout, protocol errors or command log.

### Shared authoring and project lifetime

The live session holds Project::lock from startup through connection draining
and discovery cleanup. Offline screenshot now also takes that writer lock;
force-unlock rejects a lock whose recorded PID is still running (unknown
process status fails closed). Stale locks after abrupt termination retain the
explicit offline recovery path. There is no lock bypass inside the live API.

New shared commands scene.save@1/scene.load@1 require canonical .scene.json
targets outside reserved control/asset directories (including through symlinks) and
cannot run in undo transactions. Load validates the replacement before changing
state and clears stale undo/redo actions while retaining the command log.
history.undo@1/history.redo@1 use the existing bus history operations.
Session admission stops authoring after4096logged commands or16MiB of serialized
arguments/results, checked before the next dispatch; the last completed record
is preserved. Log inspection is paged (up to64entries) and does not log itself.

All client paths pass through PathSandbox, including source imports and scene
save/load. Live reimport uses an original only when it resolves inside the
project; otherwise it uses the sandboxed project source copy. Offline reimport
retains its approved original-source behavior. Embedded NUL paths are rejected
centrally. Screenshot rendering/PNG encoding is shared with offline CLI; live
returns base64 and may also write a sandboxed .png outside reserved .engine,
assets and scenes directories. Live screenshots are capped at512x512/16frames;
the existing offline8192/1000limits remain intact.

### Verification and review

A permanent engine_rpc_fuzz target exercises raw and authenticated-envelope
parser paths in the same Clang ASan/UBSan CI job as GLB/image fuzzing. The public
live acceptance covers auth, framing/size/depth/duplicate-key errors,
disconnects, connection cap, idle-client fairness/deadlines, private permissions,
lock conflicts, sandbox escapes, command-log visibility and live-only asset
import/place/save/load/reimport/screenshot. Reference Linux/Windows results and
independent review are recorded in STATUS.md. The owner's 2026-10-31 correction
selects GPT-6 Astra high for re-review of the three prior corrections, especially
queued deadlines, and the durable journal. REVIEWER.json is the active target;
historical Claude/Astra attribution is retained.

Primary references:

- [JSON-RPC2.0 specification](https://www.jsonrpc.org/specification)
- [Windows GetSecurityInfo/read-control requirements](https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-getsecurityinfo)
- [Windows system-preferred CSPRNG](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom)

### Durable live journal (owner clarification)

M3.7 writes .engine/live-commands-<session UUID>.jsonl from startup onward. The
private file is created atomically using the same owner-only permissions as the
credential file. Its path is resolved through PathSandbox; it is not client-chosen.
The ready message exposes commandLogFile. Each new session gets a separate file;
shutdown and restart preserve previous files. Session UUIDs are independent of
authentication tokens. No raw RPC envelope or credential enters the journal.

Before a CommandBus dispatch, a started record containing name/arguments is
appended and synchronized. After dispatch, actual CommandLog entries (including
failed command results) are appended as completed records and synchronized before
a reply is sent. Records carry journalVersion1, session UUID, dispatch index, and
completed records also contain bus sequence and result. Native fsync on POSIX and
FlushFileBuffers on Windows provide the synchronization boundary. File type,
private permissions/owner and non-link status are checked on every append.

Any journal error stops further dispatch and ends the session with E_IO. Failure
before intent persistence prevents execution. Failure after execution must not be
reported as command success: outcome can be uncertain. A started record without
a matching completed record marks an uncertain command; do not automatically
replay it. A crash or partial write may leave an incomplete final JSONL line;
retain complete preceding lines and treat that tail as incomplete. A new session
never appends to that damaged file. This preserves acknowledged records across
process crashes; hardware/storage-controller power-loss guarantees are outside
the tested claim. This journal is an audit/replay source, not automatic scene
saving or an atomic transaction between asset/scene writes and command results.

The existing per-session command admission bounds remain. Durable history is
retained across sessions; aggregate disk retention/archival is not automatic.
The hard-termination/restart/error tests run on Linux and Windows.

### Claude re-review corrections

Every serialization step in command dispatch, including memory-budget accounting
and reply construction, is guarded by the journal failure path. Invalid UTF-8
filesystem names must not bypass fail-closed behavior. Journal failures drain
existing replies, print one constant credential-free stderr diagnostic and exit
with the existing E_IO status6, including for notifications without replies.
Linux non-sanitized CTest includes real started/completed fsync EIO injection
and a no-fault control; ASan/UBSan and Windows keep all their existing tests.
