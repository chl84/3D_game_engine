# Build

**Status:** M0.5. The build uses a pinned vcpkg manifest, CLI11 command
dispatch, the core library, `engine --version --json`, `engine doctor --json`
with volk-based Vulkan detection, Catch2 tests and Python contract tests. CI
pins CMake 4.4.5 and Ninja 1.13.2 and runs clang-format, clang-tidy and
ASan/UBSan.

## Local development environment

| Component | Version |
|---|---|
| OS | Omarchy 4.0.6 (Arch-based), Linux 7.2.8-9-omarchy, x86_64 |
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

## Prerequisites

- CMake 3.28 or newer
- Ninja
- A C++20 compiler: GCC 14+, Clang 18+, or MSVC 19.4+
- Python 3.10+ (test scripts and contract validation)
- `jsonschema` from `requirements-dev.txt` (installed in `.venv`)

Vulkan development packages are needed from M2 onward. They are already
installed on the development machine:

```bash
sudo pacman -S --needed vulkan-headers vulkan-validation-layers vulkan-tools vulkan-swrast ccache
```

`vulkan-swrast` provides lavapipe, the software Vulkan device used for
headless tests and golden images.

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
{"name":"engine","version":"0.1.0","gitCommit":"...","buildType":"Debug","compiler":"GNU 16.2.1","platform":"Linux-x86_64","cxxStandard":"20"}
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

Windows is **not verified** yet. The `windows-debug`, `windows-release` and
`ci-windows` presets use MSVC and vcpkg. The Windows CI job compiles and runs
headless tests from M0.2 onward; the runner has no GPU, so graphics is not
tested there. Graphics on Windows is not part of the M2/M3 approval and will be
reported as **not verified** until the owner tests it manually on a Windows
machine.

The CI workflow sets up MSVC with `ilammy/msvc-dev-cmd` and uses the
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

`main` is the default branch. Run `scripts/setup_branch_protection.sh` after
enabling GitHub Pro or making the repository public; GitHub returns HTTP 403
for branch protection and rulesets on a private repository on the Free plan.
The script requires the five CI jobs as status checks. M0.5 stays in progress
until this is decided.
