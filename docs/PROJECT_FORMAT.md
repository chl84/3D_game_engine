# Project and Scene Format

**Status:** Planned. No project or scene format is implemented yet.

The format is designed in the technical proposal and will be implemented in M1.
The planned rules are:

- UTF-8 JSON with LF line endings and no BOM
- Canonical output: sorted object keys, sorted entity arrays, stable component
  order, two-space indentation and a final newline
- A `formatVersion` integer with explicit migrations
- UUIDs for entities, assets and prefabs
- Relative paths with `/`, restricted to the project root
- Atomic saves, a project lock and a filesystem sandbox
- Right-handed coordinates: +Y up, −Z forward, metres and radians
- Quaternions stored as `(x, y, z, w)`

The implemented format, schemas and examples will replace this placeholder in
M1. Until then, no file in this repository claims to be a usable project or
scene.
