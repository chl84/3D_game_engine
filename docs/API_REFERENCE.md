# API Reference

**Status:** M1.8. Environment commands, the offline project/scene/entity/
component CLI, fixed-tick simulation, `engine schema` and `command-result@1`
exist. The reference is still handwritten; generation from the command registry
is planned for M1.9 or later.

## Implemented surface

| Command | Output | Schema |
|---|---|---|
| `engine --version --json` | Build and version metadata | `schemas/version@1.schema.json` |
| `engine doctor --json` | OS, compiler, Vulkan, devices and checks | `schemas/doctor@1.schema.json` |
| `engine schema --json` | Draft-07 schemas for built-in components | `schemas/command-result@1.schema.json` |
| `engine project ... --json` | Project create/validate/unlock result | `schemas/command-result@1.schema.json` |
| `engine scene ... --json` | Scene create/inspect/validate result | `schemas/command-result@1.schema.json` |
| `engine entity ... --json` | Entity create/delete/reparent result | `schemas/command-result@1.schema.json` |
| `engine component ... --json` | Component set/add/remove result | `schemas/command-result@1.schema.json` |
| `engine simulate ... --json` | Deterministic fixed-tick state dump | `schemas/command-result@1.schema.json` |
| `engine batch ... --json` | Batch results and undo count | `schemas/command-result@1.schema.json` |

The version output fields are `name`, `version`, `gitCommit`, `buildType`,
`compiler`, `platform` and `cxxStandard`.

`doctor` accepts repeatable `--require` values: `vulkan`, `gpu`,
`validation-layers` and `lavapipe`. A failed requirement exits with code 8.
Unknown requirement names and unknown arguments exit with code 2.

## Command result envelope

```json
{
  "status": "ok",
  "data": {},
  "warnings": []
}
```

A failed command sets `status` to `error` and includes:

```json
{
  "code": "E_VALIDATION",
  "message": "human-readable summary",
  "details": {},
  "hint": "optional next step"
}
```

## Error contract

| Error code | Exit code | Meaning |
|---|---|---|
| `E_USAGE` | 2 | Usage error, unknown command or invalid argument |
| `E_VALIDATION` | 3 | Validation error |
| `E_NOT_FOUND` | 4 | Not found |
| `E_CONFLICT` | 5 | Conflict or locked resource |
| `E_IO` | 6 | I/O or filesystem error |
| `E_UNSUPPORTED` | 7 | Unsupported format or version |
| `E_PRECONDITION` | 8 | A required capability is missing |
| `E_INTERNAL` | 1 | Internal error |
| `E_CANCELLED` | 130 | Cancelled by the user |

The codes and exit codes are stable and are part of the public contract.

## Scenes and components

The scene document format is in `docs/PROJECT_FORMAT.md`. The built-in
components are:

| Component | Fields |
|---|---|
| `engine.scene.Transform` | `position` (Vec3), `rotation` (Quat), `scale` (Vec3) |
| `engine.scene.Camera` | `fovYRadians`, `near`, `far`, `enabled` |
| `engine.scene.Motion` | `linearVelocity` (Vec3, metres per second) |

Component schemas are generated from the reflection registry by
`engine::core::ReflectionRegistry::schemas()`.

## Planned surface

From M1.8 this file is generated from the command registry and reflection
metadata. It will contain every command name and version, argument and result
JSON Schemas, error codes, examples and stability notes. The file must not be
edited by hand once generation is enabled.
