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
| [0004](ADR-0004-build-tools-and-quality-gates.md) | Pinned build tools and CI quality gates | Accepted |
| [0005](ADR-0005-error-handling.md) | Error handling with `Result` and stable codes | Accepted |
| [0006](ADR-0006-static-libraries.md) | Static libraries and module boundaries for M0–M4 | Accepted |
| [0007](ADR-0007-reflection.md) | Reflection with an explicit component registry | Accepted |
| [0008](ADR-0008-renderer-dependencies.md) | Renderer dependencies for M2 | Accepted |
| [0009](ADR-0009-scene-format-v2-mesh.md) | Scene format v2 and mesh references | Accepted |
| [0010](ADR-0010-imgui-overlay-port.md) | Dear ImGui through a local vcpkg overlay port | Accepted |
| [0011](ADR-0011-descriptor-strategy.md) | Descriptor strategy for materials and textures | Proposed |
