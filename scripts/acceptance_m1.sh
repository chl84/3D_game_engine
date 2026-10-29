#!/usr/bin/env bash
# M1 acceptance: configure, build, test a clean clone and run the CLI-only M1 script.

set -euo pipefail

repository="${1:-$(git remote get-url origin)}"
branch="${2:-$(git rev-parse --abbrev-ref HEAD)}"
workdir="$(mktemp -d "${TMPDIR:-/tmp}/engine-m1-acceptance.XXXXXX")"

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        rm -rf "$workdir"
    else
        echo "kept clean clone at $workdir/repo"
    fi
}
trap cleanup EXIT

echo "cloning $repository ($branch) into $workdir/repo"
git clone --depth 1 --branch "$branch" "$repository" "$workdir/repo"
cd "$workdir/repo"

export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}"
if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "error: VCPKG_ROOT does not point to a vcpkg checkout: $VCPKG_ROOT" >&2
    exit 1
fi

python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -r requirements-dev.txt

cmake --preset linux-debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python3"
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure

./.venv/bin/python scripts/acceptance_m1.py ./build/linux-debug/tools/engine/engine
echo "M1 clean-clone acceptance passed"
