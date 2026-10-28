# Architecture Decision Records

This directory contains the architecture decision records (ADRs) for the engine.
Each decision gets one file, numbered sequentially.

## Format

Each ADR has:

- **Status**: Proposed, Accepted, Superseded, or Rejected
- **Date**
- **Context**
- **Decision**
- **Consequences**
- **Alternatives considered**

ADRs are written in English, like the rest of the repository. A decision that
changes an accepted ADR gets a new ADR that references the old one; accepted
ADRs are not rewritten.

## Index

| ADR | Title | Status |
|---|---|---|
| [0001](ADR-0001-dependency-management.md) | Dependency management with vcpkg | Accepted |
| [0002](ADR-0002-authoritative-state-vs-simulation.md) | Authoritative state versus simulation state | Accepted |
| [0003](ADR-0003-uuid-generation.md) | UUID generation and determinism | Accepted |
