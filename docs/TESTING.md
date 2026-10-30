# Testing

**Status:** M3.8 hardening. The suite registers 27 tests on Linux without
sanitizers, 26 with sanitizers and 20 on Windows. Six window/presentation
contracts are Linux-only; the real fsync fault interposer is Linux-only without
sanitizers. Offscreen golden images and CLI/live asset acceptance run on both
platforms. No existing tests were removed or weakened. Exact results for the
current source are recorded in STATUS.md and committed verification evidence.
CI also runs clang-format 18, clang-tidy 18 with warnings as errors, ASan/UBSan,
and Vulkan detection with pinned CMake 4.4.5 and Ninja 1.13.2.

## Current tests

| Test | Type | Labels |
|---|---|---|
| `engine_core_tests` | Catch2 unit test | `unit` |
| `engine_dependencies_smoke` | Catch2 compile/link smoke test for all M0/M1 dependencies | `unit`, `dependencies` |
| `engine_asset_tests` | GLB import, hierarchy composition, image decoding, asset sidecar/cooked cache, reimport, MikkTSpace tangents, asset commands and malformed-input rejection | `unit`, `asset` |
| `engine_version_json` | CLI JSON smoke test | `unit`, `cli` |
| `engine_cli_contract` | JSON Schema contract test for version and doctor output | `contract`, `cli` |
| `engine_docs_links` | Local Markdown links and the B.10 documentation file list | `docs` |
| `engine_sensitive_scan` | Credentials, private keys, personal paths and email addresses in tracked files | `docs`, `security` |
| `engine_acceptance_m1` | CLI-only M1 acceptance: project, hierarchy, components, undo, round-trip and deterministic simulation | `contract`, `acceptance` |
| `engine_api_reference` | Generated `API_REFERENCE.md` matches `engine schema --json` | `docs`, `contract` |
| `engine_render_tests` | Headless Vulkan 1.3 device, required validation layers, descriptor-indexing minimum, single-device guard and sequential device creation | `unit`, `render` |
| `engine_window_smoke` | SDL3 window, Vulkan surface, present queue and zero validation errors | `unit`, `render` |
| `engine_swapchain_test` | Swapchain creation, frames in flight, clear and present, resize, minimize/restore and validation errors | `unit`, `render` |
| `engine_overlay_test` | ImGui debug overlay: entity list, CPU/GPU frame statistics, F1-equivalent visibility toggle, no scene mutation and zero validation errors | `unit`, `render` |
| `engine_run_smoke` | Hidden-window `engine run --frames 2` contract, window creation and clean exit | `contract`, `cli`, `render` |
| `engine_point_light_warning` | `project validate`, `engine run` and `engine screenshot` warn that PointLight is not rendered until M5 | `contract`, `cli`, `render` |
| `engine_asset_cli` | `asset import/inspect/validate/reimport/list`, dry-run, sandbox rejection, cache state and stable error codes | `contract`, `cli`, `asset` |
| `engine_screenshot_golden` | 64×64 offscreen scene compared with the lavapipe golden image | `unit`, `render`, `acceptance` |
| `engine_screenshot_cli` | `engine screenshot` creates a valid 64×64 PNG and `command-result@1` envelope | `contract`, `cli`, `render` |
| `engine_pbr_golden` | Mirrored Blender normal-map golden, Blender tangent reference and discriminating PBR probes | `render`, `asset`, `acceptance` |
| `engine_pbr_color_golden` | sRGB baseColor/emissive, linear MR/AO/normal and linear-emissive negative control | `render`, `asset`, `acceptance` |
| `engine_pbr_window` | Frame-error recovery, retirement and descriptor reuse; Linux display required | `render`, `asset` |
| `engine_asset_placement` | CLI placement, two instances, save/load, reimport, undo and cache repair | `contract`, `cli`, `asset`, `render`, `acceptance` |
| `engine_live_api` | Shared live commands, authentication, limits, lock conflicts, sandbox and authoring | `contract`, `cli`, `live`, `security`, `render`, `acceptance` |
| `engine_local_socket_deadline` | Expired queued requests cannot dispatch after a slow earlier handler | `contract`, `live`, `security` |
| `engine_live_journal` | Crash-surviving JSONL, private files and fail-closed serialization errors | `contract`, `live`, `security` |
| `engine_live_journal_fault` | Actual started/completed fsync EIO and no-fault control; Linux non-sanitized only | `contract`, `live`, `security` |
| `engine_acceptance_m3` | Public CLI/live mirrored Blender golden, reimport, stable IDs and durable log | `contract`, `acceptance`, `asset`, `live`, `render` |

`engine_version_json` runs `engine --version --json`, parses stdout as JSON and
checks the `name`, `version` and `cxxStandard` fields plus the presence of build
metadata. `engine_cli_contract` validates `engine --version --json` against
`version@1` and `engine doctor --json` against `doctor@1`, including the
top-level and per-device descriptor-indexing report, checks the
`log-record@1` fixture, verifies the `--require` exit codes, and covers
`project unlock --dry-run` with a manually created stale lock. On a machine
with a device that lacks the required descriptor features, plain `doctor`
reports `descriptor-indexing` in `requirements.failed` and exits 8; on a
machine with no Vulkan device it remains diagnostic unless `--require vulkan`
is used.
`engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11,
`tl::expected`, Catch2, volk, SDL3, VMA, GLM, Dear ImGui and fastgltf, so missing
include paths or link errors fail on both Linux and Windows.

`engine_asset_tests` imports the checked-in Blender 5.2.2 GLB fixtures
(`tests/assets/m3/`, provenance and MIT license in `PROVENANCE.md`), asserts the
node hierarchy and stable node paths, material factors, embedded PNG, sampler
state and animation-channel count, and composes world transforms in double
precision. It also mutates a valid GLB to prove stable rejection of truncated or
invalid containers, required/optional extensions, external buffers and images,
multiple scenes, non-triangle primitives, blended materials, missing attributes,
unsupported vertex attributes, hierarchy cycles and out-of-range indices. The
M3.2 importer accepts only self-contained GLB: external buffers/images, skins,
cameras, punctual lights, morph targets and texture or mesh compression fail
with an explicit error instead of being ignored. The M3.3 tests also import a
source into a project, validate the `asset@1` and `engine.asset.cooked@1`
documents against their schemas, load the cooked asset, reimport unchanged and
changed sources, compare byte-stable sidecar/cache files, and prove that the
asset UUID and stable node/mesh IDs survive reimport. PNG and JPEG decoding is
tested with valid, truncated, corrupt and oversized images. MikkTSpace tangent
generation is compared against Blender's exported tangents for a baked normal
map, including mirrored UVs, vertex splitting, degenerate UVs and preservation
of source tangents. The asset CLI contract test exercises every command,
dry-run without filesystem writes, schema validation, stable error codes,
sidecar sandbox escapes and cache-missing behavior.

`engine_render_tests` creates a headless Vulkan 1.3 device, verifies the
graphics queue and VMA allocator, requires `VK_LAYER_KHRONOS_validation` and
asserts that the debug messenger counts zero errors, checks the single-device
guard, and creates devices sequentially to prove that volk can be initialized
and finalized repeatedly. It also asserts the engine's descriptor-indexing
minimum on the real device and unit-tests the required-feature evaluator with
synthetic capabilities: a missing feature and a low limit must produce
`E_UNSUPPORTED`, stage `descriptor-indexing`, with the machine-readable
`missingFeatures` and `missingLimits` entries. There is no fallback path or
fallback test. The same executable tests the camera and scene
transform: clip-space mapping, a zero aspect guard, 8 km camera-relative model
matrices, the rotation-only camera view and frustum culling. The shaders are
compiled by `glslc` at build time and embedded as SPIR-V; CMake fails with a
clear error when `glslc` is missing. If the Vulkan loader or a usable Vulkan 1.3 device is missing, the
device cases are reported as skipped while the camera cases still run. `engine_window_smoke` creates an SDL3 window, checks the
surface extension, creates a surface and checks the present queue. It exits
with code 77 when no display server or Vulkan loader is available; CTest
reports that as skipped. `engine_screenshot_golden` renders a fixed scene offscreen with the same built-in meshes and compares RGBA pixels on lavapipe: at most 8 per channel difference, at most 1 % of channels may differ by more than 2. `engine_screenshot_cli` exercises the full CLI path, validates the PNG header
and dimensions, and rejects `--out` paths outside the project as well as
`--width 8193`, `--height 8193` and `--frames 1001`. Each rejection must carry
the exact `E_VALIDATION` code and the `path`, `maxDimension` or `maxFrames`
detail. The engine budgets are 4096 sampled images plus two samplers and 4096
material records; the 1 GiB texture budget is part of the 4 GiB total GPU
memory budget. `engine_overlay_test` creates a hidden window,
builds a camera and two meshes, renders four frames with the overlay enabled,
checks that the overlay lists every entity and submits vertices, checks that
CPU and GPU timing are reported (GPU timing is optional), toggles the overlay
off and on, and compares the scene revision and camera position before and
after rendering to prove that the overlay did not mutate authoritative state.
`engine_run_smoke` creates a project with a camera and cube, runs
`engine run --frames 2 --hidden` at 320×240, and validates the
`command-result@1` envelope. Interactive fly-camera input is verified manually
by the owner on Hyprland. `engine_swapchain_test` creates a hidden window,
presents frames, resizes the hidden window, shows it, minimizes and restores
it, renders an 8 km scene with a depth buffer and verifies that the far cube
and ground are visible while an entity behind the camera is culled, and asserts
that the swapchain follows the window and that the validation layers report no
errors. Minimization is asserted only when SDL reports the window as minimized; the Linux CI job additionally runs the test under Xvfb
with openbox and `--require-minimize`, so a window manager that ignores
minimize fails that step. It also exits with code 77 when the environment is
unavailable. The window and swapchain tests are not registered on Windows
because Windows graphics is not verified.

The Linux CI jobs select the lavapipe ICD into `VK_ICD_FILENAMES` before the
tests, so the render tests and `engine_screenshot_golden` execute on lavapipe
(the runner has no GPU). The complete CTest suite runs under Xvfb + openbox, so
`engine_window_smoke`, `engine_swapchain_test`, `engine_overlay_test` and
`engine_run_smoke` execute instead of being skipped as headless. A separate step
runs `engine_swapchain_test --require-minimize` so a window manager that ignores
minimize still fails. Linux therefore reports 18/18 with lavapipe.

The Windows job installs pinned Mesa lavapipe 26.2.6 (`mmozeiko/build-mesa`,
SHA-256 verified) and registers its ICD, so the offscreen golden and screenshot
tests and the device render tests run there too. Windows reports 13/13 CTest,
and `engine_render_tests` runs 13 cases / 105 assertions with the device cases
executing. The window, swapchain and overlay tests remain Linux-only because
Windows presentation is not verified. Without an ICD the offscreen tests report
exit code 77 and are shown as skipped; the Windows doctor step fails if
lavapipe cannot satisfy the descriptor requirements.

`engine_core_tests` covers:

- `Result<T>`/`Error` codes, JSON serialization and exit-code mapping
- UUID parsing, formatting, v4 bits and deterministic seeded generation
- filesystem sandbox escapes, symlinks and invalid roots
- JSON configuration loading, type errors and recursive merge
- JSONL logging, level filtering and required record fields
- UTC timestamp formatting at the epoch, leap day, month boundary and year boundary
- reflection registration, field metadata, JSON round-trip, partial updates,
  unknown fields, type errors and generated draft-07 component schemas
- deterministic entity UUIDs, duplicate UUID rejection, parent/child links,
  world transforms, hierarchy cycle rejection and recursive subtree destruction
- command-name validation, JSON Schema argument validation, handler success and
  failure, unknown commands, duplicate registration, command-log entries,
  undo/redo, redo invalidation, transactions and rollback
- project create/load, canonical byte-identical scene round-trip, entity
  hierarchy and components, unknown fields, newer format versions, path
  traversal and exclusive project locks
- CLI project creation, scene inspection/validation, entity hierarchy,
  component setting and batch undo, with every response validated against
  `command-result@1`
- fixed-tick motion, unchanged entities and identical state dumps from two
  identical simulation runs at 60 Hz
- `--dry-run` for state-changing CLI commands, including project create, entity
  create, component set, scene create and batch
- generated API reference parity with the command registry and reflection
  schemas
- scene format v1/v2 loading and v3 upgrade, `engine.scene.Mesh`,
  `AssetInstance`, `DirectionalLight` and `PointLight` round-trip, rejection of
  v3 components in v2 and scene version 4
- asset `asset@1`/`engine.asset.cooked@1` schemas and the stable-node/original-
  source rules (schemas are published; the M3.2 importer tests live in
  `engine_asset_tests`)
- double-precision world transforms for root entities directly at 8 km and
  16 km, plus an 8 km hierarchy with a 1 mm child offset
- `SceneView` mesh and camera access without mutation
- Doctor aggregation with a mixed device set: one supported and one unsupported
  device must pass, while all-unsupported must fail and an empty device list
  remains diagnostic

The `log-record@1` schema is in `schemas/log-record@1.schema.json` and is
validated against `tests/fixtures/log-record.jsonl` by the contract test.
Tests that cannot run on a platform use Catch2 `SKIP`, and the CI
`Core test summary` step runs `ctest -R engine_core_tests -V` so skipped cases
are visible instead of being reported as ordinary passes.

## Fuzzing

Permanent targets: `engine_gltf_fuzz` (imports arbitrary GLB bytes and composes
world transforms) and `engine_image_fuzz` (PNG/JPEG decoding). Both are built
with Clang libFuzzer, ASan and UBSan through the `linux-fuzz` preset. The
checked-in fixtures seed the corpus:

```bash
VCPKG_ROOT=... scripts/run_fuzz.sh 30
```

CI runs both targets for 30 seconds each in the `Fuzz (Clang ASan)` job. Any
crash found by fuzzing must be reduced and added as a regression test before
the finding is closed.

## CI jobs

| Job | Purpose |
|---|---|
| `Linux (ci-linux-gcc14)` | GCC 14 build, full CTest under Xvfb/openbox, strict minimize step, lavapipe detection and missing-ICD exit-code test |
| `Linux (ci-linux-clang)` | Clang 18 build, full CTest under Xvfb/openbox, strict minimize step, lavapipe detection and missing-ICD exit-code test |
| `Windows (MSVC)` | MSVC build and headless tests; no GPU |
| `Quality` | clang-format 18 and clang-tidy 18 with warnings as errors; project headers are analysed |
| `Linux ASan/UBSan` | ASan + UBSan build, CTest and Vulkan detection under the sanitizers |
| `Fuzz (Clang ASan)` | Short libFuzzer runs for GLB import and image decoding with fixture seed corpora |

CI runs on pull requests to `main`, pushes to `main` and manual dispatches.
Feature-branch pushes do not trigger CI. To verify a feature branch, open a
pull request or run:

```bash
gh workflow run ci.yml --ref <branch>
```

This keeps the full five-job matrix as the merge gate without running it on
every intermediate commit. `concurrency` cancels superseded runs on the same
ref.

Every job installs CMake 4.4.5 and Ninja 1.13.2 from the official release
archives and fails if the reported versions differ. The Linux jobs cache the
LunarG Vulkan SDK 1.4.363.0 and use its `glslc` (`shaderc v2026.4`) so Linux
and Windows produce SPIR-V with the same tool version; the check asserts that
`command -v glslc` is `$VULKAN_SDK/bin/glslc`. The Linux jobs also install
`mesa-vulkan-drivers`, `libvulkan1`, `vulkan-validationlayers` and the X11 and
Wayland development headers needed to build SDL3. They select the installed
`lvp_icd*.json` into `VK_ICD_FILENAMES`, capture the exit code from
`engine doctor --require vulkan --json` explicitly, require exit 0 and an
`llvmpipe` device, and require exit 8 when `VK_ICD_FILENAMES` and
`VK_DRIVER_FILES` point to a missing file. The selected ICD also drives the M2
render tests and golden image tests.

clang-tidy uses `HeaderFilterRegex: '.*/(engine|tools|tests)/.*'`. The paths in
`compile_commands.json` are absolute, so a regex without the leading `.*/`
silently classifies every project header as non-user code. The corrected filter
found `performance-enum-size` in `engine/core/include/engine/core/log.hpp`;
`LogLevel` now uses `std::uint8_t` as its underlying type. A local clang-tidy
run with the corrected filter exits 0, and CI verifies the same with
clang-tidy 18.

## How to run

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

Clang with warnings as errors:

```bash
cmake --preset ci-linux-clang
cmake --build --preset ci-linux-clang
ctest --preset ci-linux-clang --output-on-failure
```

AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan --output-on-failure
```

ThreadSanitizer (CPU-side tests only):

```bash
cmake --preset linux-tsan
cmake --build --preset linux-tsan
ctest --preset linux-tsan -R 'engine_(core_tests|dependencies_smoke|asset_tests|asset_cli|version_json|acceptance_m1|api_reference|docs_links|sensitive_scan)$' \
  --output-on-failure
```

This is the CPU-only scope. The old exclusion list predated PBR/live render
tests and no longer expressed that scope correctly. GPU drivers and validation
layers are not instrumented; their behavior is tested separately under
ASan/UBSan and ordinary lavapipe/window suites. All tests remain registered and
required in their normal presets. M3 commands and transport execute sequentially;
job-system race tests are deferred until that system exists in M5.

Documentation links and sensitive-data scan:

```bash
python3 tests/docs/check_links.py
python3 tests/docs/check_sensitive.py
```

The sensitive-data scan reads tracked files only, reports the pattern name and
location without printing the matched text, and fails on private keys, known
token formats, credential assignments, absolute home paths, Windows user paths,
email addresses and SSH public keys. It does not scan git history; history is
reviewed separately before a repository is made public.

Clean-clone M0 acceptance:

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
scripts/acceptance_m0.sh
```

The script clones the current branch, builds `linux-debug`, runs all five
CTest tests and validates the version and doctor JSON against their schemas.

Clean-clone M1 acceptance:

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
scripts/acceptance_m1.sh
```

The script runs every registered CTest test and the CLI-only M1 acceptance
script, which covers project creation, hierarchy, components, batch undo,
byte-identical save/load/save and deterministic simulation. The golden test
skips on a non-lavapipe device.

Windows MSVC + vcpkg:

```powershell
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --preset ci-windows --output-on-failure
```

The Windows preset requires an MSVC developer environment and
`VCPKG_ROOT` to be set. CI provides both.

## Planned test types

| Type | Purpose | Milestone |
|---|---|---|
| Unit tests | Isolated module logic | M0.2+ |
| Integration tests | Command bus, scene and serialization together | M1 |
| Contract tests | CLI JSON validates against published schemas | M0.4/M1 |
| Round-trip tests | Save → load → save is byte-identical | M1 |
| Determinism tests | Two runs with the same seed produce the same state dump | M1 |
| Sanitizer tests | ASan, UBSan and TSan | M0.2+ |
| Golden image tests | Offscreen rendering against lavapipe references | M2 |
| Vulkan validation tests | Zero validation errors in documented scenarios | M2 |

## Labels

CTest labels are used to select subsets:

- `unit`
- `dependencies`
- `integration`
- `cli`
- `contract`
- `docs`
- `security`
- `roundtrip`
- `determinism`
- `render`
- `golden`
- `slow`

Tests must not require a GPU, a window or network access unless they are
labelled `render` or `golden`.

## Not verified yet

- Windows graphics; the Windows CI job compiles and runs headless tests, but
  the runner has no GPU
- Anything that requires creating a Vulkan device in the Windows CI job;
  `doctor` only creates an instance and enumerates devices
- Windowed and overlay tests under TSan; the validation layer is not
  TSan-clean
- Device loss; resize, minimize/restore and swapchain recreation are covered,
  but the harness has no practical way to force device loss
- A fake-Vulkan end-to-end test that makes `select_physical_device` reject a
  synthetic device; the evaluator and error builder are unit-tested directly
- The console log sink; the JSONL sink is automatically tested
- Branch protection enforcement; the GitHub plan blocks it (see `STATUS.md`)

## M3.4 asset commands and tangents

`engine_asset_cli` exercises the full import/list/inspect/validate/reimport
workflow without a GPU, validates the result schemas, proves dry-run leaves
the project unchanged, checks source-copy fallback and changed-source cache
invalidation, and checks stable errors, project locks and path traversal.
Symlink checks run where the platform permits creating directory symlinks,
with an explicit NOTE otherwise; the contract test itself is never skipped.

`engine_asset_tests` adds shared command registration/logging and MikkTSpace
regressions: opposite UV orientation with vertex splitting, degenerate UVs,
invalid indices, preservation of source tangents, and per-corner comparison
against Blender's exported tangents for a model with a baked normal map.
These are CPU tests, not the deferred M3.5 image test. Both run under ASan/UBSan.
The permanent GLB fuzz target also runs tangent generation after valid import;
its C implementation is instrumented with Clang sanitizer/coverage flags in
the `linux-fuzz` preset.

## M3.6 asset instance acceptance

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset ci-linux-clang -R 'engine_asset_(tests|placement)' -V
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset linux-asan-lavapipe --output-on-failure
```

`engine_asset_placement` runs the public import/place/screenshot/reimport commands
in fresh processes. It decodes PNGs with the Python standard library and checks
visible geometry in both image halves, identical reload output and changed
output after source modification/reimport. It also checks dry-run, locking,
sandbox errors, placement undo/rollback, deletion undo, unchanged scene bytes
while rendering/reimporting, and read-only failure/recovery for a missing cache.
A duplicate GPU upload would fail, so successful two-instance rendering also
checks per-UUID resource sharing. Linux additionally renders three hidden-window
frames; Windows runs the complete offscreen scenario. No Vulkan-unavailable skip
is used by this acceptance test: its environment must provide lavapipe/device.
Existing CPU v1/v2 migration and v3 component serialization tests remain required.

The corrected color chart expects sRGB emissive red117. A deliberately linear
emissive control produces156 and must remain distinct; imported/cooked metadata
also requires sRGB and permits shared baseColor/emissive use. The inactive native
suppression rule is removed. Local driver retention remains opt-in; reference
CI uses the ordinary sanitizer configuration without retention or suppressions.

## M3.7 live API

`engine_live_api` is registered on Linux and Windows and requires lavapipe/device.
It performs live-only scene/asset authoring and validates wire/result schemas.
Negative cases include wrong/missing tokens, malformed/deep/duplicate-key JSON,
oversized/truncated frames, aborted connections, idle fairness, timeout/cap,
all offline writer lock conflicts (including screenshot and active force-unlock),
source/output/scene path escapes, protected outputs and external-source reimport
confinement. It verifies per-user POSIX0600 or a protected Windows DACL with one
current-user ACE, and checks that mutations enter the real command log without
authentication credentials. Existing CLI screenshot limits/errors remain tested.

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset ci-linux-clang -R engine_live_api -V
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset linux-asan-lavapipe --output-on-failure
scripts/run_fuzz.sh 30
```

The third permanent fuzz target engine_rpc_fuzz uses checked-in parser seeds,
raw bytes and an authenticated wrapper. No fuzz input reaches sockets, disk or
the command bus. The existing CI fuzz job runs all three targets for30s each
with ASan and UBSan halt-on-error. M3.8 adds the scheduled extended mode described below.

The engine_local_socket_deadline test uses the production platform transport
in a process fixture with stdin gates. It proves an expired queued frame cannot
dispatch after a slow accepted handler, while that handler can finish. It runs
on both platforms without server threads or POSIX-only process signals.

engine_live_journal verifies synchronized live JSONL under .engine/: intent
before execution, actual bus results before reply, process hard-kill survival,
restart retention, failed-command records, credential exclusion and fail-closed
append errors. Platform units reject unsafe link/type/permission targets.
The manual Linux post-execution fsync EIO diagnostic is recorded separately
in verification/M3_7_JOURNAL_EVIDENCE.txt; it is not claimed as a CTest/CI target.

engine_live_journal_fault is a Linux non-sanitized CTest using a test-only
LD_PRELOAD interposer for fsync on final journal files. It injects EIO at the
started/completed synchronization boundary, checks side effects and rejection
of success acknowledgement, verifies notification exit/stderr and has a no-fault
control. It is not registered on Windows or sanitizer presets; none of their
existing tests are skipped/disabled. Linux serialization regressions and the
0644 owner-writable mode test cover the actual fail-closed guards.

## M3.8 complete acceptance

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  .venv/bin/python scripts/acceptance_m3.py build/ci-linux-clang/tools/engine/engine
# A public fresh clone, optionally asserting the exact reviewed commit:
CMAKE_BUILD_PARALLEL_LEVEL=2 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  bash scripts/acceptance_m3.sh https://github.com/chl84/3D_game_engine.git m3/asset-pipeline <full-source-sha>
```

`engine_acceptance_m3` runs the Python script in Linux/Windows CI and under
ASan/UBSan. It imports the original MIT-licensed mirrored Blender coupon
without TANGENT, inspects/validates it, authors a camera and one DirectionalLight
through the CLI, places the asset and renders against the unchanged M3.5 golden.
It uses the same strict image tolerance (maximum 8, fewer than 0.1% of channels
with difference above 3). The separately tested analytic color chart remains
required; the acceptance does not regenerate goldens or relax their checks.

The test then opens the saved scene in live mode through the shipped client,
compares the actual pixels with the CLI image, saves/loads, changes the GLB
source transform and reimports. It requires a changed image and cache key,
unchanged asset/node/mesh/material/texture IDs and unchanged scene bytes.
Reloading and a final offline render must reproduce the changed image. Actual
JSONL records are schema-validated and matched to the live CommandLog.

The asset suite additionally rejects invalid UTF-8 names and POSIX source paths
(including valid aliases to invalid filenames) before any project writes.
Normal and dry-run rejection must preserve both files and directories. This
closes the asset-import leftover-directory observation from the M3.7 review.

## Extended fuzzing and corpus retention

The existing CI workflow runs only its Fuzz job at 02:23 UTC daily, for 600
seconds per GLB/image/RPC target. `workflow_dispatch` with `long_fuzz=true`
exercises the same mode on a selected branch. Ordinary required CI retains all
six jobs and the original 30 seconds per target with fresh checked-in seeds.
The two modes have independent concurrency groups. Budget: one Ubuntu job,
roughly 30 minutes of fuzzing plus setup/build per night, timeout 55 minutes.
No Windows/GPU build is scheduled in this mode.

Only this repository's MIT fixtures and synthetic RPC seeds plus their fuzzer
mutations enter the corpus. The cache contains parser input, not executables,
credentials or project/session logs. Each successful extended run saves a new
immutable cache keyed by corpus version, OS, seed/harness/dependency hash and
run/attempt; a stable prefix restores the newest compatible cache on the same
branch or default branch. A cache miss or eviction starts from original seeds.
Bump `fuzz-corpus-v1` when its layout/provenance changes. Cache storage and
inactivity eviction follow GitHub's repository cache policy; it is not archival
crash storage. The helper continues to the other targets after a target fails,
then returns failure. Logs and libFuzzer crash/timeout/OOM artifacts are uploaded
on success/failure for 14 days. A failed corpus is not saved as a successful
cache. Reproduce a saved artifact with the matching source/binary before turning
it into a named permanent regression test.

```bash
bash scripts/run_fuzz.sh 30
# Reuse built fuzzers and a persistent local corpus:
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  bash scripts/run_fuzz_targets.sh 600 /tmp/engine-fuzz-corpus /tmp/engine-fuzz-artifacts
```

The scripts enforce 1..600 seconds, a 10-second input timeout and 2048 MiB RSS
limit per process. UBSan always halts on error. The fuzz job retains its existing
`detect_leaks=0` policy; leak verification belongs to the full ASan/UBSan suite
with `detect_leaks=1`. A schedule runs from the default branch only, so nightly
activation awaits the separately approved merge. A manual extended run proves
that mode; it does not prove that a future scheduled event will fire on time.

References: [libFuzzer](https://llvm.org/docs/LibFuzzer.html),
[GitHub cache scope and immutable keys](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching),
[scheduled workflow behavior](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#schedule).
