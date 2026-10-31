# ADR-0020: M5 world data uses separate chunk files

- **Status:** Accepted storage direction by the owner on 2026-10-31; M5 design/implementation not started
- **Scope:** M5 planning; complements ADR-0019's bounded scene format

## Context

The owner approved the64MiB/depth32 project/scene parser bounds and explicitly
required M5 world data to live in separate chunk files, not in the scene file.
The planned streamed world must not turn scene JSON into an unbounded world dump.

## Decision

M5 persists world data in separate versioned chunk files. Preserve the approved
scene document bounds; do not raise or bypass them to embed the streamed world.
Any scene/project references to a world/manifest and the exact chunk/manifest
schemas remain concrete decisions for the approved M5 plan. This ADR adds no
fields, files or migrations to M4's accepted concrete contracts.

The M5 plan must define identities, format versions, bounded parsing, streaming,
atomic persistence and migration/acceptance evidence before implementation.
No directory layout, chunk size, compression or caching algorithm is selected
by this storage-direction decision.

## Consequences

Scene authoring and streamed world persistence have separate file lifetimes and
validation scopes. Chunking does not waive whole-process memory budgets or permit
silent migration writes. M4.2 remains held under the separate local binding/VM
decision; accepting this future storage direction does not authorize M5 work.
