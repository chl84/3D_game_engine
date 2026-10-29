# API Reference

**Status:** M0.6. The command bus and project/scene command schemas arrive in
M1. Version and doctor output is validated against published schemas.

## Implemented surface

| Command | Output | Schema |
|---|---|---|
| `engine --version --json` | Build and version metadata | `schemas/version@1.schema.json` |
| `engine doctor --json` | OS, compiler, Vulkan, devices and checks | `schemas/doctor@1.schema.json` |

The version output fields are `name`, `version`, `gitCommit`, `buildType`,
`compiler`, `platform` and `cxxStandard`.

`doctor` accepts repeatable `--require` values: `vulkan`, `gpu`,
`validation-layers` and `lavapipe`. A failed requirement exits with code 8.
Unknown requirement names and unknown arguments exit with code 2.

## Error contract

Every error document has:

```json
{
  "code": "E_VALIDATION",
  "message": "human-readable summary",
  "details": {},
  "hint": "optional next step"
}
```

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

## Planned surface

From M1 onward this file is generated from the command registry and reflection
metadata. It will contain:

- every command name and version
- argument and result JSON Schemas
- error codes
- examples
- stability and deprecation notes

The file must not be edited by hand once generation is enabled. Until then,
the planned command list is in `docs/AI_AGENT_GUIDE.md` and the planned format
is in `docs/PROJECT_FORMAT.md`.
