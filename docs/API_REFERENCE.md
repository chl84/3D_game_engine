# API Reference

**Status:** M0.4. The command bus and project/scene command schemas arrive in
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

## Planned surface

From M1 onward this file is generated from the command registry and reflection
metadata. It will contain:

- every command name and version
- argument and result JSON Schemas
- error codes
- examples
- stability and deprecation notes

The file must not be edited by hand once generation is enabled.
