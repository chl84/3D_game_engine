# M3.8 callback review — round 3

Reviewer: Claude Opus 5.5 high. Source `c2304ef` against `341dc3c`, base `afea88a`.

**Verdict:** callback/fixture/cache/checker delta technically approved, but
source **not publication-ready**: new Medium F2 requires a fix or an explicit
owner decision. This is not owner approval. The isolated tree was restored
clean, with no edits, commits, stash or publication at handback.

The reviewer verified validated non-owning pointer bounds/lifetime, synchronous
Mikk calls and component-copy portability. Old/new adapters produced identical
bit-level position/normal/tangent/UV/index dumps for all eight GLBs. GCC ASan/
UBSan asset tests passed 22763 assertions / 21 cases with leak checks; the new case
passed 8 assertions. The fuzz Werror build and local tidy 23 passed; PBR/color
goldens passed under the retention ASan preset on lavapipe, Blender difference 0.

Independent replays: new callbacks 4.1–4.2 seconds, old 6.4–7.4 seconds under similar
modest load (diagnostics, not benchmarks). A 90-second fresh-seed fuzz run found
nothing in 998 executions. Fixture hash/mutations and automatic seeding are
correct. The cache key hashes actual seeds. Real CTest pass/disabled/skip/empty
reports produced checker exits 0/1/1/1.

## Required F2: residual quadratic work across duplicated primitives

Four copies of the collapsed primitive took 11870 ms near idle; six copies hit
the unchanged 10-second libFuzzer timeout inside MergeVertsFast. Both inputs
were accepted and remain below the largest original seed size. Callback cost
reduction alone does not remove the search complexity or cumulative asset cost.
A position-group work check must cover adjacent Float32 values and aggregate
all primitives; rejecting or skipping previously accepted degenerate assets
requires an owner decision. The parent proposes rejection rather than a custom
tangent fallback in M3_8_TANGENT_BUDGET_PROPOSAL.md; it is not implemented.

Optional N4: update helper seed comment to include retained own mutations.
Optional N5: reference CI must measure the new seed's margin under its unchanged
10-second timeout on GitHub hardware. No reference CI was run or claimed yet.
GCC/Clang full suites, TSan, reference tidy 18/format, Windows/cache/public clone
were not rerun by this reviewer. Parent's final corrected local 600-second GLB success
arrived separately and does not invalidate F2's reproducible counterexample.
