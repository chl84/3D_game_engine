# M2 Plan — Vulkan Renderer

**Status:** Approved by the owner on 2026-10-29. M2.1–M2.8 are approved.
M2.9 adds the windowed fly-camera run command and presentation-only input; it
awaits reviewer approval and the owner's manual Hyprland test before merge.
ADR-0011 was approved with the change that descriptor indexing is a minimum
requirement and there is no fallback.

M1 is released as `v0.2.0`; M1.10 is released as `v0.2.1`. M2 starts from the
`m2/vulkan-renderer` branch. The milestone is not complete until the M2
acceptance criteria are met and recorded in `docs/STATUS.md`.

## Goal

Render a scene from M1 with Vulkan 1.3, first in a window and then offscreen
without a window. The renderer reads scene state and never mutates it.

Acceptance from `VISION.md`:

- `engine screenshot <scene> --frames N --out <file>` works without a window.
- A golden-image test passes against lavapipe references.
- The debug messenger reports zero validation errors in the documented
  scenarios, including window resize and minimization.
- Linux builds and tests are green in CI; Windows compiles and runs headless
  tests, and Windows graphics is reported as **not verified**.

## Scope

In scope:

- SDL3 window and input for the renderer demo
- Vulkan 1.3 instance, device, queues and validation layers through volk
- VMA for GPU memory
- Swapchain, presentation, frames in flight and resize/minimize handling
- Dynamic Rendering and Synchronization2
- Depth buffer, mesh rendering and a camera-relative free camera
- Scene integration: render a `scene::Scene` read-only through a `SceneView`
- Offscreen rendering, PNG screenshots and lavapipe golden-image tests
- Dear ImGui debug overlay: FPS, frame time and entity list
- Descriptor strategy ADR (for example bindless versus per-material sets)
- CI, documentation, performance measurements and M2 acceptance

Out of scope:

- glTF/GLB import and the asset pipeline (M3)
- skeletal animation, runtime UI and audio
- editor, gizmos and scene authoring (M6)
- physics, Lua and gameplay systems (M4)
- multiplayer

## Dependencies

| Dependency | Version | License | Purpose | Status |
|---|---|---|---|---|
| SDL3 | 3.4.18 | Zlib | Window, input and platform events | Approved in the milestone plan |
| Vulkan Memory Allocator | 3.5.0 | MIT | GPU memory allocation | Approved in the milestone plan |
| GLM | 1.0.3 | MIT | Matrix/vector math for the renderer | Approved in the milestone plan |
| glslc | distro package on Linux; Vulkan SDK 1.4.363 on Windows | Apache-2.0 | GLSL to SPIR-V build step | Approved in the milestone plan; only the CLI is used, not the shaderc library |
| Dear ImGui | 1.93.0-docking | MIT | Debug overlay and later editor UI | Approved in the milestone plan |
| stb_image / stb_image_write | 2026-08-02 | MIT OR Unlicense | PNG screenshots and golden-image decoding | Approved for M2 on 2026-10-29 |

`volk` and the Vulkan headers are already in the build. The Vulkan SDK remains
the source of validation layers and tools. Any dependency version change or
new transitive dependency follows A.6 and is recorded in
`docs/DEPENDENCIES.md`.

## Substeps

### M2.1 Render device and window

- Add the `render` module with explicit dependencies on `core`, `platform`,
  `scene` and later `asset`/`world`.
- Create an SDL3 window and Vulkan instance through volk.
- Enable `VK_LAYER_KHRONOS_validation` in debug builds.
- Select a physical device and graphics/present queues with capability checks.
- Create the logical device with Synchronization2 and Dynamic Rendering.
- Initialize VMA.
- Install a debug messenger with a callback that counts errors.
- Provide a headless mode that creates a device without a window.
- Add a renderer smoke test that creates and destroys the device.

### M2.2 Swapchain and frame loop

- Create the surface, swapchain, image views and per-frame synchronization.
- Use Dynamic Rendering instead of a legacy render pass.
- Clear the screen and present.
- Handle window resize, minimization, out-of-date and suboptimal swapchains.
- Keep frames in flight bounded and recreate per-frame resources safely.
- Add validation and resize/minimize integration tests.

### M2.3 First triangle

- Compile GLSL to SPIR-V at build time with `glslc`.
- Create the graphics pipeline, vertex buffer and draw path.
- Add a camera-relative view/projection matrix with GLM.
- Draw a triangle from a test scene.
- Verify zero validation errors and a stable frame.

### M2.4 Scene rendering

- Add a read-only `SceneView` over `scene::Scene`.
- Upload meshes and instance data; use a depth buffer.
- Render `Transform` hierarchies with camera-relative coordinates.
- Keep the renderer read-only: it never receives `SceneEditor`.
- Add a small built-in mesh (cube/plane) and a test scene.
- Implement frustum culling for the reference scene.

### M2.5 Offscreen rendering and screenshots

- Create offscreen images and render targets without a window.
- Evaluation result: the Windows runner has the Vulkan loader and validation
  layers from the SDK, but no ICD. This is a documented assessment, not a
  tested installation. Windows graphics stays **not verified** in M2.
  **Moved to M2.8 with owner approval on 2026-10-29:** install lavapipe or
  SwiftShader on the Windows runner. It is not implemented in M2.5.
- Evaluation result: the LunarG Linux SDK download is 366 MB per CI job, so it
  was not adopted. The Ubuntu distro `glslc` (shaderc 2023.8) produces valid
  SPIR-V and is kept for M2. **Proposed follow-up for M2.8 — requires owner
  approval:** use a cached pinned SDK `glslc` on Linux. It is not implemented
  in M2.5.
- Implement `engine screenshot <project> <scene> --frames N --out <file>`.
- Encode PNG with `stb_image_write`.
- Force lavapipe in CI with `VK_ICD_FILENAMES` and compare against golden
  images with a documented tolerance.
- Add golden images for the test scene at a fixed resolution.
- Ensure the command returns `command-result@1` and a stable exit code.

### M2.6 ImGui debug overlay

- Integrate Dear ImGui with the SDL3/Vulkan backend.
- Show FPS, CPU/GPU frame time and the entity list.
- Keep the overlay debug-only; it must not mutate authoritative state.
- Add a compile/run smoke test for the overlay.
- Implement the pre-merge screenshot hardening: resolve `--out` through
  `PathSandbox` and cap `--width`/`--height` at 8192 and `--frames` at 1000.
  This was implemented in M2.6.

### M2.7 Descriptor strategy and validation

- ADR-0011 is accepted. Descriptor indexing is a minimum requirement; there
  is no classic fallback and only one descriptor path is implemented.
- Add a device-capability query for the required descriptor features and
  limits. Device creation fails with `E_UNSUPPORTED`, stage
  `descriptor-indexing`, and machine-readable missing feature and limit lists.
- Extend `engine doctor` with the same report; `--require vulkan` exits 8 when
  no device satisfies the requirement.
- Add a test for the clear failure path using synthetic capabilities with a
  missing feature and a low limit, including the machine-readable error code
  and detail lists.
- Add validation scenarios for resize, minimize, swapchain recreation and
  device loss handling where practical.
- Ensure the debug messenger count is zero in every documented scenario.
- Document descriptor limits, the 4096-slot budget and the reference-scene
  budget. The 1 GiB texture budget is part of the 4 GiB total GPU memory
  budget.
- Assert the exact `E_VALIDATION` code and details in the `engine screenshot`
  limit tests, not only the error status. This is an owner requirement.

Implementation result: the device gate, doctor report, descriptor requirement
tests and exact screenshot error-code assertions are in place. `engine doctor
--require vulkan` exits 8 when no device meets the descriptor requirements.
The swapchain test covers resize, minimize/restore and swapchain recreation
with Required validation. Device loss is not simulated in M2.7.

### M2.8 Hardening and M2 acceptance

- Evaluate a Windows ICD for graphics tests, including prebuilt Mesa lavapipe
  packages, which are much smaller than the full SDK. The decision is made
  before M2 closes. This is the owner-approved M2.5 follow-up.
- Evaluate a cached pinned Linux `glslc` from the LunarG SDK, so Linux and
  Windows produce SPIR-V with the same tool version. This is the
  owner-approved M2.5 follow-up.
- Evaluate a compile-time switch or a separate target for the ImGui debug
  overlay, so release builds do not link it by default.
- Measure the ImGui Vulkan backend re-initialization on swapchain recreation
  and optimize it if the M2 performance budget is affected.
- Add a device-loss validation scenario where the test harness can force it;
  otherwise document why it remains uncovered.
- Clean-clone acceptance on Linux with lavapipe.
- ASan/UBSan and TSan smoke tests where meaningful; GPU drivers are not
  instrumented.
- Windows MSVC compile and headless tests; Windows graphics remains **not
  verified**.
- Performance measurement against the M2 reference budget in `ROADMAP.md`.
- Update `STATUS.md`, `TESTING.md`, `BUILD.md`, `DEPENDENCIES.md`,
  `ARCHITECTURE.md`, ADRs and `API_REFERENCE.md`.
- Reviewer approval, then owner approval for merge to `main` and tag `v0.3.0`.

Implementation result: Windows offscreen graphics now runs on pinned Mesa
lavapipe 26.2.6 (11/11 Windows CTest); Windows presentation remains not
verified. Linux CI caches the LunarG SDK 1.4.363.0 and uses its `glslc`.
`docs/PERFORMANCE.md` records the reference budget and the M5/M8 measurement
plan. The ImGui compile-time split and re-initialization measurement are
evaluated but not adopted: the overlay is off by default in release, the
re-initialization is on the swapchain-recreation path rather than the steady
state, and the editor/UI split belongs in M6. Device loss and the fake-device
end-to-end rejection test remain documented gaps.

### M2.9 Windowed run and presentation input

- Add `engine run <project> <scene>` with a WASD + mouse fly camera.
- Add Q/E vertical movement, Shift speed boost, right-mouse look and Escape to
  close.
- Add an F1 toggle for the ImGui debug overlay.
- Route SDL events to ImGui before the application handles them.
- Keep the camera pose presentation-only: pass it to `Renderer::draw_frame` as
  a `CameraPose`; never mutate the scene or authoritative state (ADR-0002).
- Add an overlay on/off test and a hidden-window `engine run` smoke test.
- The owner manually verifies the window, fly camera and overlay toggle on
  Hyprland.

## M2 reference budget

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
| Machine | i7-1265U, Iris Xe, AC power, performance profile |

Measurements use Release, validation layers off during measurement and on
during correctness tests. The budget is not changed by M2. Measurement status
and the M5/M8 plan are in [PERFORMANCE.md](PERFORMANCE.md).

## Testing

| Test | Purpose |
|---|---|
| Renderer device smoke test | Instance/device/VMA creation and teardown |
| Swapchain resize/minimize | No validation errors and correct recreation |
| Validation-layer test | Zero errors in documented scenarios |
| Golden image (lavapipe) | Deterministic offscreen output with tolerance |
| `engine screenshot` command | CLI envelope, exit codes and PNG output |
| Windows headless | Compile and run non-GPU tests; graphics not verified |
| Clean clone | Linux build, tests, golden images and acceptance script |
| Sanitizers | ASan/UBSan for CPU-side renderer code; TSan where meaningful |

CI policy remains: feature-branch pushes do not trigger CI; pull requests,
pushes to `main` and manual dispatches run the full matrix. Golden images run
on Linux with lavapipe. The Windows runner has no GPU.

## Risks

- **GPU/driver variance:** golden images differ across drivers. CI uses
  lavapipe; local GPU output is compared with a tolerance.
- **Swapchain edge cases:** resize, minimize and out-of-date handling are a
  common source of validation errors. Dedicated tests are required.
- **Shader toolchain:** `glslc` must work on Linux and Windows. Linux uses the
  distro package; Windows uses the pinned Vulkan SDK 1.4.363.
- **Dependency expansion:** SDL3, VMA, GLM, ImGui and stb expand the
  build. Each addition updates `DEPENDENCIES.md` and `THIRD_PARTY_NOTICES.md`.
- **Windows graphics:** cannot be verified on the runner. The owner must test
  manually or accept it as not verified.
- **Performance:** the reference budget is measured, not assumed; missing the
  budget is a finding, not silently waived.

## Approval requested

1. Approve the M2 scope and substeps above.
2. Approve the dependencies, including moving `stb_image_write` from M3 to M2.
3. Approve the CI policy for golden images: lavapipe on Linux, no GPU on
   Windows, Windows graphics reported as not verified.
4. Approve that M1.10 is merged and tagged `v0.2.1` before M2 starts.
5. Approve that merge, tag and `--admin` remain owner-only decisions.

No implementation starts before this plan is approved.

**Owner decision:** all five points were approved on 2026-10-29 together with
the merge and the `v0.2.1` tag.
