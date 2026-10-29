# ADR-0005: Error handling with Result and stable codes

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

Errors cross module boundaries and must be visible to the CLI, live API and AI
agents as structured data. The engine targets C++20, so `std::expected` is not
available. Throwing exceptions through module boundaries makes error paths
harder to audit and to map to stable JSON and exit codes.

## Decision

- Expected failures return `Result<T>`, based on `tl::expected`.
- Exceptions are used only for fatal initialization failures and programming
  errors. They must not cross module boundaries.
- `Error` contains a stable `code`, a human-readable `message`, a `details`
  JSON object and an optional `hint`.
- `ErrorCode` maps to stable `E_*` names and to stable process exit codes:
  `E_USAGE` 2, `E_VALIDATION` 3, `E_NOT_FOUND` 4, `E_CONFLICT` 5, `E_IO` 6,
  `E_UNSUPPORTED` 7, `E_PRECONDITION` 8, `E_INTERNAL` 1 and `E_CANCELLED` 130.
- CLI and live API serialize `Error` as JSON and use the same exit codes.
- Error codes are part of the public contract and may not change without an
  ADR.

## Consequences

- Callers must handle `Result<T>` explicitly.
- Error paths are testable and map directly to CLI JSON.
- `tl::expected` can later be replaced by `std::expected` without changing the
  public error model.
- Throwing is reserved for unrecoverable situations, which keeps normal flow
  predictable.

## Alternatives considered

- **Exceptions for expected failures:** convenient, but harder to map to
  structured results and more expensive to reason about across modules.
- **Plain integer error codes:** too little context for AI agents and users.
- **`std::expected`:** requires C++23 and is not available on the approved
  toolchain.
