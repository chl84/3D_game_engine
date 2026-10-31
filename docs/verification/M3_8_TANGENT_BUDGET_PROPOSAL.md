# M3.8 proposed tangent-generation work budget — owner decision required

**Status:** Option 1 approved by the owner on 2026-10-31, including the
artifact-test expectation change. Implementation/review is in progress; source
remains unpublished. The original proposal/alternatives are retained below.
The source delta at c2304ef is technically sound, but Claude found F2 and did
not approve publication. M3.8 is not complete. This is a decision proposal,
not the final A.5 owner report.

## Reproduced problem

The callback optimization preserves bit-identical results for all eight GLBs
(independently compared by Claude) and reduces observed replay overhead. It
does not remove upstream MikkTSpace's quadratic shared-vertex weld path when
many corners occupy the same or adjacent representable positions.

The parent's corrected long GLB run passed 36120 executions in 601 seconds;
image/RPC targets previously completed 154693/1004819 executions in 601 seconds
each. These are real local successes, not proof that every accepted GLB meets
a ten-second deadline. Claude duplicated the collapsed primitive: four copies
ran for 11870 ms near idle; six copies triggered the unchanged ten-second
fuzzer input limit in MergeVertsFast. Both sources were accepted. The initial
long-run failure is retained separately and never reported as green.

## Recommended policy: reject excessive estimated weld work

Before generating any tangents for an asset, preflight every primitive that
would invoke MikkTSpace. Preserve source tangents and the existing normal-map
selection behavior. Use a deterministic per-asset estimated-work budget:

- Group indexed corners by equal/neighboring Float32 positions, including
  signed-zero equivalence. Cover one-ULP neighbors in all three axes using
  shifted two-ULP buckets, not only exactly equal bit patterns.
- For each bucket with more than 64 corners, estimate candidate pairs as
  `n * (n - 1) / 2`. Smaller groups do not consume this dense-group budget;
  ordinary low-valence geometry retains the existing vertex/index limits.
- Use the largest estimate among the eight shifted grids for each primitive,
  then sum those estimates across **all** generating primitives in the asset.
  Duplicating primitives must not reset or bypass the asset budget.
- Proposed maximum: **1,000,000 estimated dense-group pairs per asset**.
  Reject above the limit with existing **E_VALIDATION**, before MikkTSpace or
  project writes. Include the measured estimate, limit and primitive location
  in error details. Check overflow and input validity before indexing/counting.
- Keep all current compiler/sanitizer flags, ten-second fuzz input timeout,
  per-target durations and RSS limits. No suppression or custom tangent
  fallback. Accepted inputs still use the official genTangSpaceDefault.

This is an estimate targeting the confirmed weld bottleneck, not a universal
wall-time guarantee for all importing or all hardware. Implementation review
must cover rounding/zero/subnormal edge cases so the position grouping does
not miss equivalent or unsplittable groups. Large groups at legitimate high-
valence vertices can also exceed the policy; that tradeoff needs owner approval.
Exporting valid tangents in Blender avoids missing-tangent generation, or the
source can be split/simplified. No format or cooked representation changes are
proposed; whether a future budget adjustment requires cache invalidation should
be documented explicitly rather than changing importer versions incidentally.

## Read-only survey of checked-in fixtures

A standard-library Python survey decoded indexed positions in the existing
GLBs and evaluated the proposed shifted-grid estimate; it did not change code
or run an unapproved rejection policy.

| Input requiring tangent generation | Corners | Largest equal-position group | Estimated dense pairs |
|---|---:|---:|---:|
| Mirrored Blender golden coupon | 3072 | 6 | 0 |
| Blender normal-map sphere | 672 | 16 | 0 |
| Minimal normal-map triangle | 3 | 1 | 0 |
| Retained collapsed-position artifact | 3072 | 3072 | 4717056 |
| Four copies of artifact primitive | 12288 | 3072 per primitive | 18868224 |
| Six copies of artifact primitive | 18432 | 3072 per primitive | 28302336 |
| 3072 corners alternating one Float32 step apart | 3072 | 1536 per exact position | 4717056 |

The proposed policy rejects the artifact and all duplicate variants before the
expensive call, and leaves every authored Blender golden input below budget.
The existing small collapsed-position correctness test remains supported.
The new M3.8 artifact unit test would change from expecting successful finite
tangents to asserting E_VALIDATION and unchanged input. It would retain the
actual artifact, add six-copy/adjacent-float/cumulative-budget regressions and
continue seeding the fuzzer. This change to accepted input/test expectations is
part of the requested owner decision, not already authorized by this proposal.

## Alternatives

1. **Recommended:** approve the deterministic rejection budget above. Small
   scope, explicit diagnostics and bounded work for the confirmed dense weld
   cases; some otherwise valid extreme assets require exported tangents.
2. Preserve acceptance by optimizing the official library's terminal weld
   implementation to an equivalent stable sort/map. This is a larger change
   to the approved marked build copy and needs a separate concrete patch,
   equality/edge-case evidence and independent review. The original pinned
   distribution/license would remain intact.
3. Explicitly accept the residual risk and narrow the extended-fuzz completion
   claim. Known accepted inputs still time out; this is not recommended as
   publication-ready hardening and cannot be silently counted as a green gate.

The owner selected option 1. Implement only the approved policy, obtain fresh
GPT-6 Astra high review, then publish for all six reference CI jobs, extended fuzz/cache
save/restore and public exact-SHA clone. Merge/tag and M4 remain unauthorized.

Implementation details and cache/compatibility policy are recorded in
[ADR-0015](../decisions/ADR-0015-tangent-work-budget.md). Approval of this
restriction does not approve M3.8 as complete or authorize merge/tag.
