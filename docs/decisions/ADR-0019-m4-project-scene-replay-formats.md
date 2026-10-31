# ADR-0019: M4 project, scene, profile and replay formats

- **Status:** Accepted by the owner on 2026-10-31 with the mandatory conditions below; implementation pending
- **Date:** 2026-10-31
- **Scope:** M4.1 review contract, following ADR-0012/0016/0017/0018

## Context

The owner approved the project2/scene4/replay direction, reserving approval of
the actual schemas in M4.1. Game-module identity and fixed-tick replay must be
explicit while existing authoring assets, UUIDs and commands remain compatible.

## Accepted decision and mandatory owner conditions

The owner approved the concrete contracts and the64MiB/depth32 parser exception.
Unknown component types must **always** be rejected, including absent-module
types and names not present in the active registry. The open envelope map never
authorizes unknown data or a permissive fallback. Migration changes project/scene
files only on explicit save; load/inspect/play never persist a migrated format.
M4.2 must prove these conditions with rejection and unchanged-file controls.

M4.2 is explicitly on hold while local compiler/binding and VM decisions remain
unresolved under ADR-0016. Schema approval is not permission to start that work.

Review the versioned JSON schemas, examples, migration rules and semantic
validation in [the concrete proposal](../proposals/m4/README.md).

Project2 embeds a versioned game profile. Scene4 references its ID and retains
the component map, now validated through registered storage/schema adapters.
Replay1 is bounded JSONL with a fingerprinted header, consecutive tick records
and mandatory completion footer. It records separate initial-document and
initial-runtime-state hashes, explicit runtime versions/topology, input edges,
command origins/outcomes and every completed tick's canonical state hash.

Readers must enforce registered component/command schemas and semantic limits
in addition to the envelope. Unknown/missing module types fail explicitly.
Legacy project1 and scene1–3 migration is in-memory and preserves content;
only explicit save writes the proposed formats. Existing simulate tick rates
remain independent of the fixed60Hz gameplay profile. Asset/cooked1 formats
are unchanged.

The proposal caps replay records at64KiB, replay/project/scene files at64MiB,
JSON depth at32, and declared script state at64KiB per instance. These exact
parser/format limits are owner-approved, including the explicitly documented
rejection of oversized/deep legacy documents without modification. Legacy names,
project path strings and scene-list counts otherwise retain their previous
accepted values; no new string truncation or duplicate removal is proposed.
Runtime resource starting points
are already approved. The VM-topology enum contains both alternatives solely
for review; accepting these schemas does not select a topology or ADR-0016.

M5 stores world data in separate versioned chunk files, never in the scene file.
Do not enlarge or bypass this scene bound to embed the streamed world. Exact
chunk/manifest schemas belong to the later approved M5 plan.

No production reader/writer, migration, gameplay host or replay implementation
is introduced by this decision record. A Python validator checks schema syntax,
illustrative examples and deliberate invalid envelope controls. Semantic and
runtime tests follow implementation after approval.

## Alternatives

- Keep profile identity implicit: makes a missing module or different runtime
  configuration difficult to detect before simulation.
- Serialize opaque Lua/Jolt state: ties persistence to internals and does not
  provide a portable, inspectable deterministic contract.
- Introduce binary replay immediately: less overhead, more tooling and parser
  complexity before M4 has a measured recording workload. JSONL is bounded and
  inspectable; a future format may be proposed if measurements justify it.
