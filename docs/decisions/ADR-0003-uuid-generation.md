# ADR-0003: UUID generation and determinism

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

Entities, assets and prefabs need stable identifiers that survive save/load,
reimport and renaming. The engine also promises deterministic headless runs:
the same scene, input, seed, build and platform must produce the same result.

If a simulation creates entities with UUIDs from OS entropy, two otherwise
identical headless runs diverge. Explicit IDs are possible in commands, but
they are not practical for every runtime spawn.

## Decision

- **Authoring and normal creation:** UUID v4 from OS entropy.
- **Simulation and headless runs:** UUIDs are produced by a `UuidGenerator`
  seeded with the explicit simulation seed, a stream identifier and a counter.
  The generator sets the version and variant bits, so every value is a valid
  v4-shaped UUID.
- **Explicit IDs:** commands may carry an explicit UUID for tests, imports and
  migrations.
- **No timestamp semantics:** UUIDs are opaque identifiers. They must not be
  parsed for time or used for ordering.

The same build, platform, seed, command order and stream assignment must
produce the same UUID sequence. Tests must cover:

- two headless runs with the same seed produce identical UUIDs
- two runs with different seeds produce different UUIDs
- authoring UUIDs are valid v4 values
- serialization round-trips preserve UUIDs

## Consequences

- Headless reproducibility includes entity creation.
- UUIDs created during simulation are not cryptographically random; that is
  acceptable because they are identifiers, not secrets.
- Stream identifiers must be assigned deterministically by the systems that
  create entities.
- The generator is part of the deterministic core and must not use
  `std::random_device` during a seeded run.

## Alternatives considered

- **UUIDv7:** includes a timestamp and is sortable, but timestamp-based IDs
  complicate seeded determinism and are not needed for stable identity.
- **Random v4 everywhere:** simple, but breaks deterministic headless runs when
  entities are created during simulation.
- **Explicit IDs only:** too cumbersome for runtime spawning.
