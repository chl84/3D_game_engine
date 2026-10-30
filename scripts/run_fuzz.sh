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

corpus="$(mktemp -d)"
trap 'rm -rf "$corpus"' EXIT
mkdir -p "$corpus/gltf" "$corpus/image" "$corpus/rpc"
cp tests/assets/m3/*.glb "$corpus/gltf/"
cp tests/assets/m3/*.png tests/assets/m3/*.jpg "$corpus/image/"

./build/linux-fuzz/tests/engine_gltf_fuzz "$corpus/gltf" \
    -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048
./build/linux-fuzz/tests/engine_image_fuzz "$corpus/image" \
    -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048

cp tests/fuzz/corpus/rpc/* "$corpus/rpc/"
./build/linux-fuzz/tests/engine_rpc_fuzz "$corpus/rpc" \
    -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048
