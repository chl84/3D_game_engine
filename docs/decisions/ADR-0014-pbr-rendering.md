# ADR-0014: Shared PBR rendering and GPU resource lifetime

- **Status:** Accepted — implementation of the owner-approved M3.5 plan
- **Date:** 2026-10-30

## Context

M3.5 renders cooked GLB materials using ADR-0011, authored DirectionalLight and
read-only scene access. Both swapchain and offscreen rendering must use the
same material implementation. CLI asset placement and automatic resolution of
project assets are still M3.6 work.

## Decision

`PbrRenderer` owns the shared pipeline/resources. Both renderers expose additive
explicit upload, retirement and collection methods. Callers upload `LoadedAsset`
objects and scenes reference their existing `AssetInstance.assetId`. This does
not introduce another scene format or a second material descriptor strategy.
Legacy colored built-in geometry remains the M2 debug/sample path; it is not a
texture/material fallback. PBR uses the same mandatory device capability gate.

### Descriptors and samplers

- Set 0: a uniform buffer per frame slot containing the authored world-space
  light direction/radiance and fixed linear ambient 0.03.
- Set 1: material/texture-record SSBO, two fixed samplers and a runtime sampled
  image array with 4096 slots. The image array is the highest binding and has
  VARIABLE_DESCRIPTOR_COUNT, PARTIALLY_BOUND and UPDATE_AFTER_BIND flags.
- This uses **4099** descriptors in the update-after-bind pool: 4096 sampled
  images, two samplers and one SSBO. The previous gate's 4098 count omitted the
  SSBO; the gate and doctor now account for it. No additional Vulkan feature
  bit is required. Device-wide reservations prevent multiple PBR heaps from
  exceeding the advertised pool limit.
- Image and sampler descriptors are separated to honor the approved two-sampler
  budget. Nearest/linear filtering use fixed repeat samplers, with glTF wrap
  modes applied per mip level and explicit nearest/trilinear mip selection.
  Derivatives are computed before alpha discard. Clamped/mirrored coordinates
  are constrained to half-texel edges per level; repeat uses hardware wrapping.
- 4096 material records. A default record is allocated only when primitives
  omit their material; 4096 explicitly assigned materials still fit.

Texture, material and device pool limits are checked before allocation. Missing
textures, invalid references and non-finite material/mesh data fail explicitly.
Meshes use persistently mapped, flushed vertex/index buffers. Textures use a
staging buffer, GPU images, synchronization2 barriers and linear mip blits.
Base-color images use SRGB; data images use UNORM. Identical pixel data and
color space within one asset share image backing while retaining separate
sampler metadata and descriptor slots.

### Lifetime and budgets

Uploads wait for device idle before changing shared material records. Retirement
moves the complete asset into a queue retaining images, buffers and slot leases.
Collection waits for device idle (which covers every frame fence) before freeing
that queue; no retired slot is reused early. This is a conservative synchronous
safe point, not asynchronous streaming. M5 can replace the idle collection with
finer fence/timeline retirement without changing asset identity or descriptors.

All VMA memory blocks count toward the 4 GiB tracked GPU budget, including empty
reserved capacity. Allocation helpers also enforce that budget on legacy mesh,
depth and offscreen/readback resources. Actual VMA texture allocation sizes count
against the 1 GiB texture sub-budget. Descriptor pools reserve 64 bytes per
entry; WSI images reserve twice their pixel storage plus 1 MiB per image, and
the ImGui backend reserves 64 MiB. The overlay checks peak vertex/index and
texture requirements against that reservation before backend uploads. These
native/descriptor reservations are conservative accounting estimates, not a
measurement of opaque driver memory. Whole-process/reference-scene measurement
remains M5/M8 as agreed with the owner.

### Material and lighting equations

Metallic-roughness uses GGX distribution, correlated Smith visibility and
Schlick Fresnel (dielectric F0 = 0.04), with Lambertian diffuse and roughness
clamped to 0.045. Tangents are transformed with the linear model matrix and
orthogonalized against the inverse-transpose normal. Handedness accounts for
negative determinant transforms. Back-face handling respects double-sided
materials; the complete mapped normal is reversed for back-face lighting.
Alpha OPAQUE and MASK are supported, without blended transparency.

The first enabled DirectionalLight supplies linear radiance; more than one
fails explicitly. Without one, only fixed ambient and emissive remain. Ambient
is multiplied by occlusion and the nonmetal diffuse color. The tone map is
Reinhard (`linear / (1 + linear)`), followed by the standard piecewise sRGB
transfer. UNORM targets encode explicitly; SRGB targets encode in the attachment,
so there is exactly one output conversion. As approved in ADR-0012, baseColor is
sRGB while the remaining inputs, including emissive textures, are linear.

Shaders are built by glslc for Vulkan 1.3 with SPIR-V 1.5. This avoids requiring
optional shaderDemoteToHelperInvocation for alpha discard. Each embedded shader
pair has unique C++ symbols; legacy/PBR arrays must not share inline names.

### Hierarchy and cache correction

Renderer node matrices are composed in double, including affine/sheared
hierarchies. Instance translation is combined with node translation in double;
the camera position is subtracted before conversion to Float32. Renderers never
mutate the scene. Tests include an identical image at 16 km.

Cooked loading previously lost the selected glTF scene roots. Sidecars now store
stable root UUIDs in the existing free-form `metadata.sceneRoots` field, and
loading restores only those roots (not all parentless nodes). The sidecar/cooked
formats stay v1; `gltf-import@3` invalidates older derived caches. Reimport retains
asset/internal IDs and rebuilds corrupt derived buffers even if the key matches.

## Verification

The Blender mirrored-UV model without tangents is compared with a fixed golden
and the Blender-tangent export. Negative controls remove normal mapping and flip
tangent signs. Numerical probes cover sRGB versus linear textures, the MR blue
channel, occlusion, emissive/tone mapping, alpha modes, SRGB mip blits and the last
slot of full 4096-image/material tables. Window tests exercise two-frame
retirement/collection/reuse. Validation is Required for the PBR tests.

## References

- [Vulkan descriptor indexing sample](https://docs.vulkan.org/samples/latest/samples/extensions/descriptor_indexing/README.html)
- [Vulkan descriptor binding rules](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorBindingFlagBits.html)
- [glTF material specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials)
