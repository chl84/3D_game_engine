#!/usr/bin/env bash
# Fresh-clone M3 acceptance. Optional third argument pins the expected source SHA.
set -euo pipefail
repository="${1:-$(git remote get-url origin)}"
branch="${2:-$(git rev-parse --abbrev-ref HEAD)}"
expected="${3:-}"
workdir="$(mktemp -d "${TMPDIR:-/tmp}/engine-m3-acceptance.XXXXXX")"
cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        rm -rf "$workdir"
    else
        echo "kept clean clone at $workdir/repo"
    fi
}
trap cleanup EXIT
git clone --depth 1 --branch "$branch" "$repository" "$workdir/repo"
cd "$workdir/repo"
actual="$(git rev-parse HEAD)"
echo "M3 clean-clone source: $actual"
if [[ -n "$expected" && "$actual" != "$expected" ]]; then
    echo "error: clean-clone source differs from expected commit" >&2
    exit 1
fi
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}"
if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "error: VCPKG_ROOT does not point to a vcpkg checkout" >&2
    exit 1
fi
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
cmake --preset linux-debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python3"
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure --output-junit "$workdir/ctest-m3.xml"
.venv/bin/python - "$workdir/ctest-m3.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET

cases = list(ET.parse(sys.argv[1]).getroot().iter("testcase"))
if not cases:
    raise SystemExit("error: clean-clone CTest report contains no tests")
not_run = [case.get("name", "unknown") for case in cases
           if case.find("skipped") is not None or case.get("status") in ("disabled", "notrun")]
print(f"M3 clean-clone CTest: {len(cases)} tests, {len(not_run)} skipped/disabled/not run")
if not_run:
    raise SystemExit("error: incomplete clean-clone acceptance; not run: " + ", ".join(not_run))
PY
.venv/bin/python scripts/acceptance_m1.py ./build/linux-debug/tools/engine/engine
.venv/bin/python scripts/acceptance_m3.py ./build/linux-debug/tools/engine/engine
echo "M3 clean-clone acceptance passed"
