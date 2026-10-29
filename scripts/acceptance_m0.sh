#!/usr/bin/env bash
# M0 acceptance: configure, build and test a clean clone on Linux.
#
# Usage:
#   scripts/acceptance_m0.sh [repository-url] [branch]
#
# Defaults to the current origin and the current branch. Set KEEP=1 to keep the
# temporary clone for inspection.

set -euo pipefail

repository="${1:-$(git remote get-url origin)}"
branch="${2:-$(git rev-parse --abbrev-ref HEAD)}"
workdir="$(mktemp -d "${TMPDIR:-/tmp}/engine-m0-acceptance.XXXXXX")"

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

./build/linux-debug/tools/engine/engine --version --json > version.json
./build/linux-debug/tools/engine/engine doctor --json > doctor.json

.venv/bin/python - <<'PY'
import json
from pathlib import Path

import jsonschema

root = Path.cwd()
for document_name, schema_name in (
    ("version.json", "version@1.schema.json"),
    ("doctor.json", "doctor@1.schema.json"),
):
    document = json.loads((root / document_name).read_text(encoding="utf-8"))
    schema = json.loads((root / "schemas" / schema_name).read_text(encoding="utf-8"))
    jsonschema.Draft7Validator(schema).validate(document)
    print(f"{document_name} validates against {schema_name}")
PY

echo "M0 clean-clone acceptance passed"
