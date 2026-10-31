# ADR-0015: Asset-wide tangent-generation work admission

- **Status:** Accepted by the owner; implementation/review in progress
- **Date:** 2026-10-31
- **Related:** [ADR-0012](ADR-0012-asset-pipeline-and-scene-v3.md),
  [M3.8 approved proposal](../verification/M3_8_TANGENT_BUDGET_PROPOSAL.md)

## Context

Extended fuzzing found an expensive shared-vertex weld in official MikkTSpace.
Cheaper callbacks preserved exact results but did not remove quadratic work:
repeating a collapsed primitive six times still exceeded the unchanged ten-
second input limit. The owner approved a deterministic admission budget,
including changing the new artifact regression to require controlled rejection.
This is an explicit restriction on previously accepted extreme source assets.

## Decision

Before any primitive is generated, validate and preflight **all** primitives
that require missing-tangent generation. Preserve the existing material/source-
tangent selection. Invalid counts/indices or non-finite positions fail first.
All admission state is local to one asset invocation; no asset geometry changes
before the entire preflight succeeds.

Use indexed-corner position groups in eight shifted two-code grids. Float32
codes are monotone, canonicalize signed zero and cover adjacent representable
values across bin boundaries. At very small magnitudes, use a minimum code
spacing of the smallest normal Float32 value: FTZ/DAZ can collapse subnormal
bbox spans even when the endpoints themselves are normal. This conservative
coarsening also handles subnormals and the transition back to ULP spacing.
It may group more tiny coordinates than an exact IEEE-equality weld would.

For a bucket containing more than 64 corners, charge `n*(n-1)/2`; buckets of
64 or fewer contribute zero. Take the maximum grid estimate per primitive,
then sum those maxima across meshes/primitives. The approved maximum is
**1,000,000 estimated dense-group pairs per asset**, inclusive. Stop as soon as
a grid's prefix exceeds the remaining budget. The reported `estimatedWork`
is then an observed lower bound, not a completed total for the whole input.

Reject with E_VALIDATION, constant explanatory text, Blender-export/simplify
hint and details `estimatedWork`, `limit`, `meshIndex`, `primitiveIndex` and
`jsonPointer`. Preflight precedes MikkTSpace and project writes, including
reimport and dry-run. Use one balanced integer-keyed tree at a time to avoid
hash-collision attacks and eight simultaneous copies of grouping state. Validated
index limits bound per-group arithmetic; remaining-budget subtraction prevents
asset-total overflow.

Accepted inputs continue through the pinned official `genTangSpaceDefault` and
existing per-corner vertex splitting/sign handling. No custom fallback, library
patch, dependency/version change, compiler/sanitizer change or relaxed fuzz
limit. Source tangents and materials without normal maps retain their behavior.

## Compatibility and caches

The owner explicitly approved this source-input restriction and the changed
artifact-test expectation under A.6. High-valence/dense but valid geometry may
need exported tangents or simplification. Small degenerate fixtures remain
supported; authored Blender normal-map/golden models are below the budget.

Project1/scene3/asset1/cooked1 and importer@4 are unchanged. Accepted output is
identical; existing cooked caches remain loadable because loading does not run
MikkTSpace. Reimport of an over-budget source now fails without altering its
previous cache/sidecar/copy. A future budget adjustment must be documented as an
admission-policy change; it does not silently change tangent output or force
cache regeneration merely to enforce a generation-time bound.

## Verification and limitations

Permanent tests cover the exact retained artifact, six duplicated primitives,
aggregate cost within/across meshes, the inclusive million-pair boundary,
64-corner exemption, all eight adjacent-Float32 grid phases, signed zero,
subnormals/tiny spans, extreme finite values, invalid positions and unchanged
asset geometry on rejection. CLI tests cover original/six-copy import and
reimport, normal/dry-run, with project/cached-file snapshots. Existing Blender
tangent/image comparisons remain required. Fuzzers retain the artifact seed.

This is an estimate for the confirmed dense welding bottleneck, not a universal
CPU-time deadline or a replacement for existing vertex/index/source limits.
Ordinary small position groups are exempt; other linear import/generation work
can still depend on asset size and hardware. Reference CI, fresh extended fuzz
and GPT-6 Astra high review must pass before publication is declared complete.

## Alternatives considered

- Skip tangent generation for degenerate inputs: risks a custom Blender-
  incompatible fallback; rejected.
- Optimize the third-party terminal weld: larger marked-copy change and new
  equality/edge-case proof burden; not selected by the owner.
- Accept residual timeouts or relax fuzz limits: not selected; keep the guards.
