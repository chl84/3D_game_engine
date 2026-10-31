#!/usr/bin/env bash
# Build and run the short libFuzzer targets with the checked-in seeds.
# Usage: scripts/run_fuzz.sh [seconds-per-target]
set -euo pipefail
# Undefined behavior must stop a fuzz run, not merely print a recoverable warning.
export UBSAN_OPTIONS="${UBSAN_OPTIONS:+$UBSAN_OPTIONS:}halt_on_error=1:print_stacktrace=1"

seconds="${1:-30}"
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

cmake --preset linux-fuzz
cmake --build --preset linux-fuzz --target engine_gltf_fuzz engine_image_fuzz engine_rpc_fuzz

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
bash scripts/run_fuzz_targets.sh "$seconds" "$temporary/corpus" "$temporary/artifacts"
