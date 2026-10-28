# API Reference

**Status:** Generated in M1. The command bus and command schemas do not exist
yet.

## Implemented surface

| Command | Output |
|---|---|
| `engine --version --json` | Build and version metadata as JSON |

The output fields are `name`, `version`, `gitCommit`, `buildType`, `compiler`,
`platform` and `cxxStandard`.

## Planned surface

From M1 onward this file is generated from the command registry and reflection
metadata. It will contain:

- every command name and version
- argument and result JSON Schemas
- error codes
- examples
- stability and deprecation notes

The file must not be edited by hand once generation is enabled.
