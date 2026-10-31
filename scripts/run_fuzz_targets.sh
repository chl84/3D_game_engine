#!/usr/bin/env bash
# Run built fuzzers with an optional persistent corpus and retained diagnostics.
# Usage: scripts/run_fuzz_targets.sh seconds corpus-directory artifact-directory
set -euo pipefail
seconds="${1:?seconds per target required}"
if [[ ! "$seconds" =~ ^[1-9][0-9]*$ ]] || (( ${#seconds} > 3 )) || (( seconds > 600 )); then
    echo "error: seconds per target must be an integer in 1..600" >&2
    exit 2
fi
corpus="${2:?corpus directory required}"
artifacts="${3:?artifact directory required}"
mkdir -p "$corpus" "$artifacts"
corpus="$(cd "$corpus" && pwd)"
artifacts="$(cd "$artifacts" && pwd)"
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:+$UBSAN_OPTIONS:}halt_on_error=1:print_stacktrace=1"
mkdir -p "$corpus/gltf" "$corpus/image" "$corpus/rpc"
# Original MIT fixtures, retained own fuzz mutations and synthetic RPC seeds
# enter the cache as untrusted parser input, never tools or dependencies.
cp tests/assets/m3/*.glb "$corpus/gltf/"
cp tests/assets/m3/*.png tests/assets/m3/*.jpg "$corpus/image/"
cp tests/fuzz/corpus/rpc/* "$corpus/rpc/"
failed=0
for target in gltf image rpc; do
    mkdir -p "$artifacts/$target"
    echo "Fuzz target $target: seconds=$seconds, corpus files=$(find "$corpus/$target" -type f | wc -l)"
    if ! "./build/linux-fuzz/tests/engine_${target}_fuzz" "$corpus/$target" \
        -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048 \
        -artifact_prefix="$artifacts/$target/" 2>&1 | tee "$artifacts/$target.log"; then
        failed=1
    fi
done
exit "$failed"
