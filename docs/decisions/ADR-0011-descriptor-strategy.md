# ADR-0011: Descriptor strategy for materials and textures

- **Status:** Proposed
- **Date:** 2026-10-29

## Context

M2 renders untextured built-in meshes with push constants. M3 adds the glTF/GLB
asset pipeline and therefore materials and textures. The renderer must choose
how material data and texture views are made available to shaders before M3
starts, because the choice affects shader interfaces, descriptor allocation,
draw-call batching, streaming and hot reload.

The engine targets Vulkan 1.3 on Linux and Windows. The verified graphics
targets are Intel Iris Xe through Mesa/ANV and lavapipe. Windows graphics is
**not verified** because the CI runner has no ICD. The M2 reference budget is
at most 100 unique PBR materials and 200 draw calls per frame; M3 should not
need more than that initially.

The relevant Vulkan 1.3 facilities are:

- **Classic descriptor sets:** one or more sets with explicitly bound uniform,
  storage and combined-image-sampler descriptors. Per-material sets are bound
  before each draw.
- **Descriptor indexing (bindless):** arrays of descriptors indexed in the
  shader, with update-after-bind and partially-bound descriptors. A single
  set can cover many textures and material records.
- **Buffer device address (BDA):** 64-bit GPU addresses passed in push
  constants or storage buffers. It removes some buffer bindings but does not
  bind textures.

## Measured support

The following values were read with `vulkaninfo` on the development machine on
2026-10-29. Both devices report Vulkan 1.4.354 through the 1.4.363 loader; the
engine only requires Vulkan 1.3. Descriptor indexing and buffer device address
are core Vulkan 1.2 features, so a Vulkan 1.3 driver exposes them as optional
core features through `VkPhysicalDeviceVulkan12Features`; the engine does not
require an extension for either.

| Capability | Iris Xe (ANV, Mesa 26.2.5) | lavapipe (LLVM 23.1.3) |
|---|---|---|
| `descriptorIndexing` | true | true |
| `shaderSampledImageArrayNonUniformIndexing` | true | true |
| `descriptorBindingPartiallyBound` | true | true |
| `descriptorBindingVariableDescriptorCount` | true | true |
| `descriptorBindingSampledImageUpdateAfterBind` | true | true |
| `runtimeDescriptorArray` | true | true |
| `maxPerStageDescriptorUpdateAfterBindSampledImages` | 201 326 592 | 1 015 808 |
| `maxDescriptorSetUpdateAfterBindSampledImages` | 201 326 592 | 1 015 808 |
| `maxUpdateAfterBindDescriptorsInAllPools` | 201 326 592 | 4 294 967 295 |
| `maxBoundDescriptorSets` | 32 | 8 |
| `bufferDeviceAddress` | true | true |

The Linux CI job runs lavapipe (LLVM 20.1.2) and passes the M2 render tests.
The exact limits may differ from the local LLVM 23.1.3 build, so the engine
must query every feature and limit at runtime instead of hard-coding these
numbers.

Both target devices support all three alternatives. Capacity is therefore not
the deciding factor: even classic descriptor sets expose large image limits on
these drivers. The deciding factors are binding overhead, material/texture
growth in M3, descriptor lifetime during streaming, and portability to the
unverified Windows driver.

## Alternatives considered

### A. Classic descriptor sets

One global set for per-frame data and one set per material containing its
uniform/storage data and combined image samplers. The renderer binds the
material set before each draw.

Advantages:

- Simplest to implement and validate; no optional feature detection.
- Works on every Vulkan 1.3 device.
- Easy to reason about descriptor lifetimes and updates.

Disadvantages:

- One bind per material change; batching is limited by material order.
- Descriptor pools must be sized and recycled per material; hot reload and
  streaming need set rewrites or new sets.
- The number of simultaneously bound sets is limited (`maxBoundDescriptorSets`
  is 8 on lavapipe and 32 on Iris Xe), which pressures future global,
  material, shadow and post-process sets.
- Scaling to many texture variants requires more sets and more binds, even
  though the raw image limits are large.

### B. Descriptor indexing (bindless)

A bounded array of combined image samplers and a storage-buffer array (or one
material-record storage buffer) live in one set that is bound once per frame.
Shaders index the arrays with a material/texture ID. Updates use
`VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT` and partially bound descriptors.

Advantages:

- One bind per frame; draw calls can be batched across materials and textures.
- Natural fit for M3's glTF material/texture import and streaming: a texture
  slot is allocated once and updated in place.
- Avoids per-material descriptor-set pools and the 8/32 bound-set limit.
- Both verified devices support every required feature.

Disadvantages:

- Requires feature detection and a portability fallback; Windows graphics is
  unverified.
- Descriptor lifetime is explicit: a texture must stay alive while any draw
  can reference its slot. Streaming and hot reload need a free-list and
  synchronization.
- Update-after-bind and non-uniform indexing are easy to get wrong; validation
  coverage must include bounds, empty slots and updates during frames in
  flight.
- The engine must define and enforce its own slot budgets rather than relying
  on the driver's large advertised limits.

### C. Buffer device address

Pass 64-bit addresses of vertex, index, instance or material data in push
constants or storage buffers instead of binding those buffers.

Advantages:

- Fewer buffer binds; useful for GPU-driven culling, meshlets and large-world
  geometry.
- Both verified devices support it.

Disadvantages:

- Does not solve texture binding, which is the main M3 problem.
- Requires careful alignment, lifetime and robustness handling.
- Adds a second resource-access mechanism before the engine has a demonstrated
  need for GPU-driven geometry.
- Better evaluated with M5's streaming and large-world work, where indirect
  and GPU-driven rendering become relevant.

## Decision (proposed)

Adopt **descriptor indexing (bindless) as the primary descriptor strategy** for
M3 materials and textures:

- One persistently bound bindless set contains a bounded array of combined
  image samplers and a storage buffer with material records. The existing
  per-frame data remains in its own set, so a material draw binds at most the
  per-frame set and the bindless set.
- Texture and material slots are allocated from engine-owned free lists.
  Descriptors are updated after bind; partially bound descriptors cover unused
  slots.
- The engine budget is 4096 sampled images and 4096 material records, well
  below the measured limits. The budget is validated against
  `maxPerStageDescriptorUpdateAfterBindSampledImages`,
  `maxDescriptorSetUpdateAfterBindSampledImages`,
  `maxUpdateAfterBindDescriptorsInAllPools` and `maxBoundDescriptorSets` at
  device creation.
- Shaders use `nonuniformEXT` indexing for the texture array. Material records
  are indexed by a 32-bit material ID carried in the draw's push constants or
  instance data.

Keep a **classic per-material descriptor-set fallback** for devices that do
not expose the required descriptor-indexing features. The fallback uses one
global set plus one set per material and the same material-record layout. Both
verified target devices use the primary path; the fallback exists for the
unverified Windows driver and future low-end devices.

Do **not** adopt BDA for M3. Re-evaluate BDA in M5/M8 for GPU-driven geometry,
indirect draws and large-world streaming, where its benefits are concrete.

## Consequences

### M3

- The glTF importer creates material records and texture slots. Texture
  creation registers a view and sampler in the bindless heap; material
  creation writes a material record.
- Draw calls carry material IDs; the renderer binds the global set once and
  can batch draws across materials without rebinding descriptor sets.
- Texture streaming and hot reload update existing slots after bind. A
  slot must not be reused until all frames that referenced it have completed;
  the renderer needs a deferred-free queue tied to frame fences.
- The fallback path needs its own descriptor pool and per-material set
  management. It must produce the same material record layout so shaders and
  tests can cover both paths.
- M3 tests should cover: full and partially filled texture arrays, unused
  slots, material-ID bounds, texture lifetime across frames in flight, and a
  forced-fallback build.

### M2.7

M2.7 is the decision and validation substep, not the material implementation.
After owner approval it should:

- Accept this ADR and record the decision in `ARCHITECTURE.md` and
  `STATUS.md`.
- Add a `DeviceCapabilities` query for the descriptor-indexing and BDA
  features and the limits above; fail with a clear `E_UNSUPPORTED` error when
  a required primary-path feature is missing and the fallback is unavailable.
- Add validation scenarios for resize, minimize, swapchain recreation and
  device loss where practical, and assert zero debug-messenger errors.
- Assert the exact `E_VALIDATION` code and detail keys in the `engine
  screenshot` limit tests, not only the error status.
- Document the engine budgets and the reference-scene descriptor budget.

Material and texture implementation starts in M3. No bindless descriptor heap
is added to the M2 renderer unless it is needed to validate the capability
query.

### Risks

- **Unverified Windows driver:** the fallback path is the portability answer,
  but Windows graphics remains not verified until an ICD is available. The M2.8
  Windows ICD evaluation must include the descriptor-indexing feature report.
- **Descriptor lifetime:** streaming and hot reload are the main source of
  use-after-free bugs. The deferred-free queue is mandatory.
- **Validation coverage:** lavapipe and ANV can differ in update-after-bind
  validation. The M3 tests must run on both.
- **Performance:** bindless reduces binds but can be slower on some tilers.
  Measure in M3 and M8; do not assume a win.
- **Budget drift:** the 4096-slot budget is an engine limit, not a driver
  limit. Enforce it in code and tests.

## Approval requested

1. Approve descriptor indexing as the primary strategy for M3 materials and
   textures, with a classic-set fallback.
2. Approve deferring BDA to M5/M8.
3. Approve the M2.7 deliverables listed above, including the exact error-code
   assertions in the screenshot limit tests.

Implementation of M2.7 starts after this ADR is accepted.
