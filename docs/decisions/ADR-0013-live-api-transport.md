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
