# Performance

**Status:** M2.8. This document records the reference-scene budget, what is
measured today, and what remains for M5/M8. The M2 renderer does not yet
implement the reference scene, so M2 does not claim budget compliance.

## M2 reference budget

The budget is unchanged from the approved M2 plan and targets the reference
machine: Intel Core i7-1265U, Iris Xe, AC power, performance profile.

| Parameter | Budget |
|---|---|
| Resolution | 1920×1080 |
| Frame time | ≤ 16.6 ms |
| Entities | ≤ 1 000 |
| Triangles per frame | ≤ 250 000 after frustum culling |
| Draw calls per frame | ≤ 200 including the shadow pass |
| Lights | 1 directional + 4 point lights; 1 shadow-casting directional light |
| Materials | ≤ 100 unique PBR materials |
| Texture memory | ≤ 1 GiB, a sub-budget of the total GPU memory |
| Total GPU memory | ≤ 4 GiB, including textures, material records and descriptor pools |
| CPU time | ≤ 8 ms |
| GPU time | ≤ 12 ms |
| Descriptor budget | 4096 sampled images + 2 samplers; 4096 material records; 2 bound sets |

Measurements use Release builds, validation layers off during measurement and
on during correctness tests.

## What is documented and measured now

Documented now:

- The budget above, including the 4 GiB total GPU memory limit and the 1 GiB
  texture sub-budget.
- The descriptor-indexing minimum and the 4096-image/4096-material budgets in
  ADR-0011.
- The CPU/GPU frame-time reporting in the ImGui overlay (`FrameStats`) and the
  `engine_overlay_test` smoke test.

Measured in M2.8:

- The small overlay test scene at 800×600 with three entities and two built-in
  meshes. Local Intel Iris Xe results are approximately CPU 1.2–1.8 ms and GPU
  0.09–0.13 ms per frame. Lavapipe results are approximately CPU 0.7–0.9 ms
  and GPU 2.2 ms.
- The swapchain resize/minimize scenarios at small window sizes. These are
  correctness tests, not budget measurements.

These numbers are **not** the reference scene. M2 has no 1 000-entity load
scene, no PBR materials or textures, no lights, no shadow pass and no
250 000-triangle scene. No M2 statement should compare these numbers with the
16.6 ms frame budget.

## What must be measured in M5/M8

M5 owns the open-world/reference scene content; M8 owns final performance and
packaging. The measurement plan is:

1. Build the reference scene with 1 000 entities, a frustum-culled triangle
   count up to 250 000, at most 100 PBR materials, 1 directional + 4 point
   lights, one shadow-casting directional light and at most 200 draw calls.
2. Render at 1920×1080 in a Release build on the reference machine with
   validation layers disabled.
3. Record CPU frame time, GPU timestamp time, draw calls, triangle count,
   material/texture counts, texture memory and total GPU memory.
4. Run the same scene with validation layers enabled and assert zero debug
   messenger errors.
5. Compare against the budget. Missing a budget is a finding, not a silent
   waiver; record it in `STATUS.md` and the milestone report.
6. Measure the ImGui overlay's re-initialization cost on swapchain recreation
   and the cost of the debug overlay in debug builds. Release builds do not
   enable the overlay by default.

M3 adds basic PBR materials and one directional light so golden-image tests can
verify glTF materials. M5 adds shadows and point lights together with the
reference scene and streaming content. M5 is the first milestone where the
entity/triangle/material budget can be exercised realistically. M8 repeats the
measurement with the final renderer, descriptor path and packaging.
