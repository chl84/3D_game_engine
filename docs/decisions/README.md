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
| [0011](ADR-0011-descriptor-strategy.md) | Descriptor strategy for materials and textures | Accepted |
| [0012](ADR-0012-asset-pipeline-and-scene-v3.md) | Asset pipeline, scene format v3 and PBR lighting | Accepted |
| [0013](ADR-0013-live-api-transport.md) | Local live API transport | Accepted |
| [0014](ADR-0014-pbr-rendering.md) | Shared PBR rendering, descriptors and resource lifetime | Accepted |

[ADR-0015](ADR-0015-tangent-work-budget.md) records the owner-approved M3.8
asset-wide tangent work budget and its explicit source-compatibility tradeoff.

## M4 proposals — awaiting owner approval

| ADR | Title | Status |
|---|---|---|
| [0016](ADR-0016-lua-bindings.md) | Lua bindings and VM boundary | Proposed |
| [0017](ADR-0017-deterministic-jolt.md) | Deterministic Jolt profile for M4 | Proposed |
| [0018](ADR-0018-game-module-linking.md) | Static game modules and installed SDK | Proposed |
