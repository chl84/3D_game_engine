# ADR-0007: Reflection with an explicit component registry

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

Components need one source of truth for serialization, JSON Schema, the object
inspector, Lua bindings and the generated API reference. The M1 plan allows
`entt::meta`, code generation or another mechanism, provided the choice is
recorded in an ADR.

The engine already uses EnTT for the ECS. `entt::meta` would add a second
registration mechanism and couples the reflection metadata to the EnTT
version. Code generation would add a build step before the component set is
stable.

## Decision

Implement a small explicit reflection registry in `engine/core`:

- Components are registered once at startup with a stable name such as
  `engine.scene.Transform`.
- Fields are registered with member pointers, a name, a type and an optional
  description.
- Supported field types in M1 are `bool`, `int`, `float`, `double`,
  `std::string`, `Vec3` and `Quat`. Unsupported types fail at compile time.
- The registry generates draft-07 JSON Schema per component, with all fields
  required, `additionalProperties: false`, default values and descriptions.
- The registry provides type-erased `to_json` and `from_json` functions.
  `from_json` rejects unknown fields and supports partial updates, so a command
  can change one field without replacing the whole component.
- EnTT remains the ECS storage. The scene module registers component factories
  and uses the reflection registry for JSON conversion and schemas.

## Consequences

- No additional reflection dependency and no build-time code generation in M1.
- Registration is explicit and easy to audit, but must be kept in sync with
  the component definitions.
- Schemas and serialization are deterministic because fields are emitted in
  registration order and object keys are sorted by the JSON writer.
- A future code generator can emit the same registration calls without changing
  the public format.
- `entt::meta` remains an option if the inspector or Lua bindings need more
  metadata later; that would be a new ADR.

## Alternatives considered

- **`entt::meta`:** integrates with EnTT, but adds a second registration API,
  has a smaller feature set than a purpose-built registry and couples metadata
  to the EnTT version.
- **Code generation from headers:** removes manual registration, but adds a
  build tool and a generated-code dependency before the component model is
  stable.
- **Hand-written serializers per component:** simple at first, but duplicates
  field names and types and drifts from the schema.
