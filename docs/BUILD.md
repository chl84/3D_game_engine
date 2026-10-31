# Build

**Status:** M3 is released as v0.4.0. M4.1 admission remains held on the local
binding/compiler decision described below. The build uses a pinned vcpkg manifest, CLI11 command
dispatch, the core library with the reflection registry, the command bus with
transactions and undo/redo, the scene/ECS module, the project/scene format with
atomic saves, the offline project/scene/entity/component CLI, fixed-tick
simulation, `engine schema`, the M2 Vulkan renderer with an SDL3 window,
glslc-compiled shaders, offscreen rendering, `engine screenshot`, PNG golden
images, the `stb_image_write` encoder, the Dear ImGui debug overlay and the
mandatory descriptor-indexing device gate. Version, doctor and all offline
commands support `--json` and
state-changing commands support `--dry-run`. Catch2, contract, render and M1
acceptance tests cover the CLI and the renderer, and
`scripts/generate_api_reference.py` keeps `API_REFERENCE.md` generated. CI pins CMake 4.4.5 and Ninja 1.13.2 and runs
clang-format, clang-tidy and ASan/UBSan. Documentation links and
sensitive-data checks run as CTest tests.

## Local development environment

| Component | Version |
|---|---|
| OS | Omarchy 4.x (Arch-based), Linux x86_64 |
| CPU | Intel Core i7-1265U, 10 cores / 12 threads |
| GPU | Intel Iris Xe Graphics (ADL GT2), Mesa 26.2.5, Vulkan 1.4.354 |
| RAM | 30 GiB local |
| GCC | 16.2.1 |
| Clang | 23.1.3 |
| CMake | 4.4.4 |
| Ninja | 1.13.2 |
| Python | 3.14.8 |
| vcpkg | bootstrapped at `~/.local/share/vcpkg`, commit `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |

The CI target environment is Ubuntu 24.04 with GCC 14 and Clang 18, plus
Windows Server 2022 with MSVC. M0.5 installs `g++-14` and uses the
`ci-linux-gcc14` preset; the ASan/UBSan job also uses `g++-14`. CI is the
source of truth for reproducible builds; local Omarchy versions may be newer
and are reported here.

M4.1 adds an owner-required local gate: sol2 must build with default GCC16 and
Clang23 before adoption. The current unmodified3.3.0 admission pin fails in
`optional<T&>::emplace` on both. A diagnostic newer upstream pin builds all engine
targets with both compilers, but it is not adopted and does not change normal
presets/manifests. See [evidence, alternatives and recommendation](verification/M4_1_LOCAL_COMPILER_OPTIONS.md).
M4.2 is explicitly held pending that binding decision and VM-topology approval.

## Prerequisites

- CMake 3.28 or newer
- Ninja
- A C++20 compiler: GCC 14+, Clang 18+, or MSVC 19.4+
- `glslc` for the GLSL to SPIR-V build step. The build fails with a clear
  error if it is missing.
- Python 3.10+ (test scripts and contract validation)
- `jsonschema` from `requirements-dev.txt` (installed in `.venv`)

Vulkan development packages are needed from M2 onward. They are already
installed on the development machine:

```bash
sudo pacman -S --needed vulkan-headers vulkan-validation-layers vulkan-tools vulkan-swrast ccache
```

A minimal sample project is in `samples/m2-demo`. After building, run it with:

```bash
./build/linux-debug/tools/engine/engine run samples/m2-demo scenes/main.scene.json
```

Use `WASD`, `Q`/`E`, Shift, the right mouse button, `F1` for the overlay and
`Escape` to close.

`vulkan-swrast` provides lavapipe, the software Vulkan device used for
headless tests and golden images. The validation layers are required by the
`required validation layers report no errors` test; CI installs
`vulkan-validationlayers` on Ubuntu. Devices must also expose the
descriptor-indexing features and limits in ADR-0011; there is no classic
fallback, and `Device::create` rejects an unsupported device with
`E_UNSUPPORTED`.

`glslc` is a build tool, not a linked library. On Arch it comes with the
`shaderc` package. Linux CI caches the LunarG Vulkan SDK 1.4.363.0 and uses its
`glslc`; Windows CI installs the same LunarG SDK and adds its `Bin` directory
to `PATH`. `VULKAN_SDK` is set on both, and CMake searches `PATH` and
`VULKAN_SDK` and fails at configure time when `glslc` is missing.

SDL3 is built by vcpkg with the Vulkan, X11 and Wayland features. On Debian and
Ubuntu the build needs the X11 and Wayland development headers:

```bash
sudo apt install -y libx11-dev libxft-dev libxext-dev libwayland-dev \
    libxkbcommon-dev libegl1-mesa-dev
```

CI installs these before configuring. Windows builds need no extra system
packages; the Vulkan loader is optional and graphics is not verified there.

## vcpkg

The manifest is `vcpkg.json`, pinned to baseline
`0e5eb43501dab17aa055dd54bd7a8c018f330fe2`. Bootstrap it once:

```bash
git clone https://github.com/microsoft/vcpkg "$HOME/.local/share/vcpkg"
"$HOME/.local/share/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
```

The CMake presets use `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`.
The first configure installs the pinned dependencies into
`build/<preset>/vcpkg_installed/`.

## Python development environment

The contract tests validate CLI JSON against the schemas in `schemas/`. Create
the development environment before configuring CMake:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
```

On Windows PowerShell:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
```

CMake prefers `.venv` over the system Python when it exists.

## Configure and build

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

The binary is at:

```text
build/linux-debug/tools/engine/engine
```

To use Clang:

```bash
cmake --preset ci-linux-clang
cmake --build --preset ci-linux-clang
ctest --preset ci-linux-clang --output-on-failure
```

To use GCC explicitly:

```bash
cmake --preset ci-linux-gcc
cmake --build --preset ci-linux-gcc
ctest --preset ci-linux-gcc --output-on-failure
```

The CI presets set `ENGINE_WARNINGS_AS_ERRORS=ON`, so warnings fail the build.

## Sanitizers

AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan --output-on-failure
```

ThreadSanitizer:

```bash
cmake --preset linux-tsan
cmake --build --preset linux-tsan
ctest --preset linux-tsan --output-on-failure
```

ASan/UBSan and TSan cannot be combined. Sanitizers are GCC/Clang only.

## Formatting and static analysis

```bash
clang-format --dry-run --Werror $(find engine tools tests -type f \( -name '*.cpp' -o -name '*.hpp' \))
clang-tidy -p build/ci-linux-clang engine/core/src/version.cpp tools/engine/src/main.cpp
```

CI uses clang-format and clang-tidy 18; the development machine has version 23.
The CI version is authoritative for formatting.

## Version command

```bash
./build/linux-debug/tools/engine/engine --version --json
```

Expected shape:

```json
{"name":"engine","version":"0.4.0","gitCommit":"...","buildType":"Debug","compiler":"GNU 16.2.1","platform":"Linux-x86_64","cxxStandard":"20"}
```

## Doctor

```bash
./build/linux-debug/tools/engine/engine doctor
./build/linux-debug/tools/engine/engine doctor --json
./build/linux-debug/tools/engine/engine doctor --require vulkan --json
```

`doctor` reports the OS, compiler, project root, Vulkan loader and instance,
physical devices, validation layers and lavapipe. It exits 0 by default even
when Vulkan is missing, so it can run on a Windows CI runner without a GPU.
`--require` accepts `vulkan`, `gpu`, `validation-layers` and `lavapipe`; a
failed requirement exits with code 8.

The JSON output is validated against `schemas/doctor@1.schema.json`.

## Windows

The `windows-debug`, `windows-release` and `ci-windows` presets use MSVC and
vcpkg. Windows compilation, headless contracts and offscreen rendering with
pinned lavapipe are verified by CI (M3.7: 19/19 on d0797e1). This includes
Blender normal-map and sRGB/linear golden images and live journal/security
contracts. Physical Windows presentation and hardware-GPU rendering remain
**not verified** until the owner tests them manually. Current M3.8 results
are recorded separately in STATUS.md; earlier results do not certify new code.

The CI workflow locates Visual Studio with `vswhere`, imports
`vcvarsall.bat x64` and verifies `cl.exe` before configuring. It uses the
`x64-windows` vcpkg triplet. The first configure installs the dependencies in
`build/ci-windows/vcpkg_installed/`.

## Toolchain versions

CI pins CMake 4.4.5 and Ninja 1.13.2. Every job downloads the official
Kitware and Ninja release archives, prepends them to `PATH`, and fails if
`cmake --version` or `ninja --version` does not match the pinned values.

The development machine has CMake 4.4.4 and Ninja 1.13.2. A clean local build
with CMake 4.4.5 and Ninja 1.13.2 passed all four CTest tests, so the local
4.4.4 difference does not hide an unverified CI path. CI remains the source of
truth for reproducible builds.

## Privacy and repository hygiene

Tracked files must not contain hostnames, usernames, absolute home paths,
email addresses, credentials, tokens or private keys. `engine_sensitive_scan`
checks tracked files and fails on those patterns. The scan does not replace a
review of git history before a repository is made public.

The environment table keeps the reference CPU, GPU and RAM needed for the
performance budget. Exact kernel and host-specific details are intentionally
not recorded.

## M0 clean-clone acceptance

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
scripts/acceptance_m0.sh
```

The script clones the current branch from `origin` into a temporary directory,
creates `.venv`, configures and builds `linux-debug`, runs all CTest tests, and
validates `engine --version --json` and `engine doctor --json` against
`version@1` and `doctor@1`. It exits non-zero if any step fails. Pass a
repository URL and branch as arguments to test another checkout, or set
`KEEP=1` to keep the clone for inspection.

## M1 clean-clone acceptance

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
scripts/acceptance_m1.sh
```

The script clones the current branch, builds `linux-debug`, runs all seven
CTest tests and then executes the CLI-only M1 acceptance script. The acceptance
script creates a project, builds an entity hierarchy, sets components, runs a
batch with undo, checks byte-identical save/load/save and compares two fixed-
tick simulation dumps.

## Memory

The development machine has 30 GiB of RAM. The project reference requirement
remains 16 GB, and the provisional memory budget remains 4 GB for the game
process in the reference scene. The local 30 GiB is a development convenience,
not a change to the target.

## Vulkan verification

```bash
vulkaninfo --summary
```

The development machine reports:

- Intel Iris Xe Graphics (ADL GT2), Vulkan 1.4.354, Mesa 26.2.5
- llvmpipe (lavapipe), Vulkan 1.4.354, Mesa 26.2.5
- `VK_LAYER_KHRONOS_validation` 1.4.363

To force lavapipe:

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary
```

## CI triggers

CI runs on pull requests to `main`, pushes to `main` and manual dispatches.
Feature-branch pushes do not trigger CI, so intermediate commits do not spend
runner time. To verify a feature branch before opening a pull request:

```bash
gh workflow run ci.yml --ref <branch>
```

The full five-job matrix is the merge gate and runs on every pull request and
every push to `main`.

## CI Vulkan and lavapipe

The Linux CI jobs install:

```bash
sudo apt-get install -y libvulkan1 mesa-vulkan-drivers vulkan-tools
```

They then run `engine doctor --require vulkan --json`, capture the exit code
explicitly before parsing the JSON, and require exit 0 plus an `llvmpipe`
device. A second run with `VK_ICD_FILENAMES` and `VK_DRIVER_FILES` pointing to
a missing file must exit 8. The ASan/UBSan job runs the same successful
detection under the sanitizers. This also provides lavapipe for the M2 golden
image tests.

## Branch protection

`main` is the default branch and is protected. The repository is public, so
branch protection is available on the Free plan. Running
`scripts/setup_branch_protection.sh` applies the rule with these required
checks: `Linux (ci-linux-gcc14)`, `Linux (ci-linux-clang)`, `Windows (MSVC)`,
`Quality` and `Linux ASan/UBSan`. Force pushes and deletions are disabled, and
conversations must be resolved before merging. `enforce_admins` is off so the
owner can still push directly during M0.

## M3 PBR verification

The shared PBR shaders compile for Vulkan 1.3 / SPIR-V 1.5 so alpha discard does
not require optional shaderDemoteToHelperInvocation. Generated embedded shader
symbols are unique for each pair. No new build/runtime dependency is added.

`engine_pbr_golden` uses the checked-in 256x192 lavapipe golden
`tests/fixtures/golden/m3_pbr_256.png` and the locally baked mirrored-UV Blender
model without exported tangents. It also compares Blender's exported tangents,
checks sensitivity to disabled normals/flipped handedness, reflection and 16 km,
and runs numerical color-space/mip/alpha, wrap/filter, reflected-normal and full
descriptor-boundary probes. Blender reference/golden checks share strict byte
tolerances; the analytical reflection probe rejects a missing handedness sign.
Vulkan validation is Required; initialization errors fail the test. A different
GPU skips only this lavapipe-specific CTest (77), not the window lifetime test.

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset ci-linux-clang --output-on-failure
# Local physical-GPU controls, without fixed-golden comparison:
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/intel_icd.json build/ci-linux-clang/tests/engine_pbr_test --device-smoke
```

`engine_pbr_window` checks two-frame descriptor/resource retirement and reuse on
Linux with a display (CI uses Xvfb). It also verifies that invalid scene data can
be corrected and rendering resumes without a fence/semaphore hang. Windows runs the offscreen golden/probes;
Windows presentation remains unverified. Use the existing sanitizer preset
`linux-asan`. Whole-process/reference-scene performance remains M5/M8.

## Local Mesa RTASM leak-check policy

The earlier 112-byte LSan report is identified as Mesa 26.2.5 RTASM arena metadata,
not an unknown engine allocation. See the [source/stack diagnosis](verification/M3_SANITIZER_DIAGNOSIS.md)
and [full M3.5 report](verification/M3_5_STATUS_REPORT.md).
The opt-in local test preset preserves native-library globals using the supported
Vulkan loader switch. The inactive LSan rule was removed at the owner's request. Leak detection,
ASan/UBSan and Required Vulkan validation remain enabled. The ordinary preset
and Ubuntu CI do not inherit this local policy.

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset linux-asan-lavapipe --output-on-failure
```

The explicit `engine_pbr_color_golden` now compares a fixed 192x128 six-patch
image as well as numerical sRGB/data expectations. It is registered on Linux
and Windows. This is separate from the Blender normal-map golden.

## M3.7 native local transport

No third-party dependency is added. The planned control target contains the
bounded JSON parser; platform uses POSIX sockets/process APIs on Linux and
Windows system ws2_32/advapi32/bcrypt for sockets, DACLs and CSPRNG. Windows
private discovery permissions are verified before writing credentials and
checked independently by the cross-platform integration test. The new
engine_rpc_fuzz target runs alongside the existing GLB/image targets.

## M3.8 acceptance entry points

The normal build includes `engine_acceptance_m3` on Linux and Windows. It drives
only the public CLI/live interface with the checked-in mirrored Blender fixture
and the existing M3.5 golden; no Blender installation is needed to run it.
The fixture source, generator and MIT provenance remain checked in.

```bash
cmake --preset ci-linux-clang
cmake --build --preset ci-linux-clang
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset ci-linux-clang
# Clean clone, expected SHA optional as the third argument:
bash scripts/acceptance_m3.sh https://github.com/chl84/3D_game_engine.git m3/asset-pipeline
```

Select a working lavapipe ICD and a display for the complete Linux window suite;
CI uses Xvfb/openbox. `KEEP=1` retains the fresh clone for diagnosis. The clean
clone runs full CTest plus explicit M1 and M3 acceptance. It reports JUnit
test/skip totals and rejects incomplete acceptance with any skipped, disabled
or unexecuted CTest test. No release version is
changed before owner approval of merge/tag. The nightly fuzz mode reuses the
pinned CI toolchain and adds no package; see TESTING.md for limits and cache
provenance. Hardware performance goals remain M5/M8 and are not inferred from
software-rendered acceptance timings.
