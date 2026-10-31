# MikkTSpace

Unmodified upstream `mikktspace.c` and `mikktspace.h` from
[mmikk/MikkTSpace](https://github.com/mmikk/MikkTSpace), commit
`3e895b49d05ea07e4c2133156cfa94369e19e409` (header version 1.0).
License: zlib; the notice is preserved in both source files and `LICENSE`.
Approved by the owner on 2026-10-30.

| File | SHA-256 |
|---|---|
| `mikktspace.c` | `de87e74107df766ce68108801262bd8d53899414236b59810509a8fc2a51e288` |
| `mikktspace.h` | `17fc433894f24c73753d548086cc4d8c5c0379f4a6edfb98b5da243e4f0bc3d0` |

CMake verifies these hashes at configure. The sources build as a private C
static library with the same sanitizer instrumentation as the engine. Engine
warning and formatting rules apply to the C++ adapter, not upstream sources.
There are no transitive dependencies and no network fetch at build time.

## Build-copy correction

UBSan found an upstream rotate-by-zero bug in the random pivot selection at
lines 1460 and 1667: `uSeed >> (32-t)` shifts by 32 when `t == 0`. CMake changes
only that expression to `uSeed >> ((32-t)&31)` in the generated build copy.
This implements the intended 32-bit rotation for every shift count, leaves
the checked-in upstream sources intact and does not disable any sanitizer.
The generated copy is an altered source version; this alteration is described
here and in the build file, as required by the zlib notice.

The permanent GLB fuzz target also exposed a NaN-to-int cast in upstream's
`FindGridCell` for fully collapsed position bounds. The build copy returns cell
zero for a zero extent and computes differences in double to avoid overflow
for finite Float32 positions. This only hardens the internal vertex-welding
spatial hash. A collapsed-position unit regression exercises the case with
UBSan enabled. These corrections are not upstreamed or claimed to be upstream.
