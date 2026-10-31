# ADR-0019: M4 project, scene, profile and replay formats

- **Status:** Proposed — concrete schemas await owner approval
- **Date:** 2026-10-31
- **Scope:** M4.1 review contract, following ADR-0012/0016/0017/0018

## Context

The owner approved the project2/scene4/replay direction, reserving approval of
the actual schemas in M4.1. Game-module identity and fixed-tick replay must be
explicit while existing authoring assets, UUIDs and commands remain compatible.

## Proposed decision

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
parser/format limits require owner approval, including the explicitly documented
rejection of oversized/deep legacy documents without modification. Legacy names,
project path strings and scene-list counts otherwise retain their previous
accepted values; no new string truncation or duplicate removal is proposed.
Runtime resource starting points
are already approved. The VM-topology enum contains both alternatives solely
for review; accepting these schemas does not select a topology or ADR-0016.

No production reader/writer, migration, gameplay host or replay implementation
is introduced by this proposal. A Python validator checks schema syntax,
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
