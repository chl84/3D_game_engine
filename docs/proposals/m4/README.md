# Approved M4 format contracts — implementation pending

**Owner-approved on 2026-10-31**, including the64MiB/depth32 document bounds.
Unknown component types must always be rejected by registered type validation;
migration writes project/scene files only on explicit save. These draft-07
contracts are not yet installed in `schemas/` or integrated into readers/writers.
M3 still reads/writes project1 and scene3. Both VM topology values remain review
alternatives; neither is selected. M4.2 is explicitly held pending ADR-0016's
local-build/binding and VM decisions.

| Contract | Concrete change |
|---|---|
| [project2](project@2.schema.json) | Preserve project1 fields/settings; add embedded, versioned `gameProfile`. |
| [game-profile1](game-profile@1.schema.json) | Explicit profile ID, required static modules/schema hashes, 60Hz, pinned physics profile, VM topology and approved quotas. |
| [scene4](scene@4.schema.json) | Preserve scene3 envelope/entity/component representation; add `gameProfileId` and reflected physics/script components. |
| [replay1](replay@1.schema.json) | Bounded JSONL header, consecutive tick records and mandatory successful footer. |
| [common](common@1.schema.json) | UUID/hash/seed/path/vector/state definitions shared by these proposals. |

The files in [examples/](examples/) illustrate syntax only. Replay hashes are
placeholders, and the referenced script/scene paths are not a runnable sample.
The two game-profile examples deliberately differ only in VM topology.

## Compatibility and validation contract

M4.2 will migrate project1 and scene1–3 in memory. Preserve all UUIDs, component
data, settings, metadata and existing asset/cooked formats. New fields get a
stable engine-only profile (empty game-module list) unless explicitly changed.
Migration does not initialize physics, alter existing Motion integration, or
write files. Only explicit save writes project2/scene4. Existing `simulate`
continues to use `settings.tickRate` (1..1000); `play`/replay uses the separate
profile's fixed60Hz. Reject a profile mismatch before starting play.

Legacy project/scene/entity names and `engineVersion` remain unrestricted JSON
strings, including empty and long names. Legacy project path strings and scene
lists retain their accepted values, including duplicate list entries; there is
no new1024-character or65,536-item schema cap on legacy fields. Project load
preserves strings; resolving an actual filesystem operation still goes through
PathSandbox. A formerly storable but unusable path is not rewritten or silently
dropped. The1024-character path cap applies only to newly introduced script and
replay fields. Registered component field defaults materialize omitted legacy
fields on in-memory migration, as the existing reader/writer does. Optional
legacy metadata/parent fields become `{}`/`null`; stored values are preserved.

**Explicit owner-approved compatibility exception:** the new64MiB document/depth32
parser bounds can reject formerly loadable oversized/deep legacy documents.
Do not truncate, split, rewrite or save them automatically. Reject before world
construction/writes with a structured resource-limit validation error containing
path, actual size/depth, proposed limit and advice to keep using M3 or request an
approved larger bound. The owner explicitly approved this exception. No new
parser limit is applied to production readers by this decision record. M5 world
data belongs in separate chunk files, not in a larger scene document.

Validation has two required stages: this envelope, then registered descriptor
schemas and semantic checks. Existing built-in component field contracts remain
unchanged. The open component map allows registered `game.*` types without an
engine switch; it does **not** permit arbitrary unrecognized objects. A missing
module/type/schema or unknown field is a structured error, never dropped data.
Validate module versions/schema hashes and unique module IDs before world load.
Profile IDs must resolve to the exact profile whose canonical hash is recorded.

Additional semantic checks before allocating runtime bodies or opening scripts:

- UUID uniqueness, existing parents, acyclic hierarchy and finite numbers.
  Preserve legacy stored quaternion values; require normalized nonzero rotation
  only when constructing a physics body/character. Reject unsupported physics
  scale (anything other than positive unit scale) with entity/component/field
  details. Ordinary M3 visual transforms are not subject to new physics limits.
- Rigid bodies require exactly one primitive collider. Colliders require a body;
  characters own their capsule and cannot also have a body/collider. Dynamic
  bodies and characters must be roots. Reject Motion/body/character transform
  writer conflicts. Non-dynamic mass is ignored; dynamic mass must be positive.
- Script component paths are relative to `scriptRoot` (`player.lua` in the
  scene example); replay dependency paths are project-relative
  (`scripts/player.lua`). PathSandbox rejects absolute,
  traversal and symlink escapes before I/O. Source is text only. Initial state
  must match the script's declared state schema. Paths are never native modules.
- State/metadata are finite JSON, with depth at most32; declared script state
  has at most256 root fields and at most64KiB encoded UTF-8 per instance.
  Propose a64MiB cap on each project/scene document and validate size/depth before
  parsing or allocating an unbounded tree. These parser limits are owner-approved.

## Replay framing and determinism

Proposed limits are64KiB per UTF-8 JSON record (excluding newline),64MiB per
file, depth32 and at most4,096 combined queued events/commands per tick. Array
count limits do not override the tighter byte limit. Duplicate JSON keys,
nonfinite values, blank/unknown record types and incomplete records are errors.
Only LF/CRLF terminated records are accepted. A final newline is required.

There is exactly one header, then ticks starting at0 without gaps/duplicates,
then exactly one footer with `ticks` equal to the count and `lastStateHash`
matching the final tick (or the initial canonical state when count is0).
EOF without a successful footer is an incomplete recording, not a valid replay.
Errors report the last completed tick and failure phase outside this success
format. This does not promise rollback of a partially executed tick.

Before stepping, compare initial-scene/profile/input-map hashes, engine/binary/
compiler/numeric fingerprints, runtime versions/topology, seeds and the exact
UUID/path-sorted asset/script/module dependency sets and hashes. Reject missing,
extra or duplicate dependencies. Seeds are unsigned64-bit integers encoded as
16 hex digits. A different VM topology changes the profile hash.

Input action names come from the hashed map. `down` is the post-transition held
set; `pressed`/`released` are consistent edges against the preceding tick (empty
held set initially). Reject contradictory edges and nonfinite/out-of-range axes.
Commands have a globally increasing, gap-free sequence starting at0; validate
arguments/results using the existing versioned command and `command-result@1`
schemas. The recorder audits actual deterministic dispatch outcomes. Playback
must reproduce/check them, not apply both a recorded command and its live-script
duplicate. `origin: generated` commands are matched against recorded outcomes;
only `origin: external` inputs are injected. Both use the shared command bus.

Hashes use SHA-256 of canonical explicit state: UUID-sorted transforms, velocities,
activation/character state, declared script/game state, ordered pending work and
RNG state. Normalize negative zero, reject nonfinite values, sort object keys
lexicographically by UTF-8 bytes, preserve array order, and encode numeric values
with the engine's documented shortest round-trip JSON representation. Never hash
pointers, padding, unordered traversal or an opaque Lua/Jolt heap. M4.3 must add
byte-level golden vectors and mutation tests before claiming interoperability.

## Review checkpoint

The owner approved the concrete field contracts, parser bounds, migration
defaults and replay framing with mandatory unknown-type rejection and explicit-
save-only migration. VM topology and local compiler/binding compatibility remain
separate unresolved M4.1 decisions; M4.2 must not start before they are resolved.
Validation of these proposal examples is not an engine format implementation:

```bash
.venv/bin/python3 scripts/validate_m4_proposals.py
```
