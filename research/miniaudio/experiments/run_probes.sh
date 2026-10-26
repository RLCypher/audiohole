#!/usr/bin/env bash
set -euo pipefail
research_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd -- "$research_dir"
expected_revision=9634bedb5b5a2ca38c1ee7108a9358a4e233f14d
actual_revision="$(git -C upstream/miniaudio rev-parse HEAD)"
if [[ "$actual_revision" != "$expected_revision" ]]; then
    echo 'Upstream revision differs from the archived evidence.' >&2
    exit 1
fi
if ! git -C upstream/miniaudio diff --quiet HEAD -- miniaudio.h; then
    echo 'Upstream header has local changes; preserve baseline evidence.' >&2
    exit 1
fi
mkdir -p work
probe_flags=(-std=c99 -O2 -Wall -Wextra -Wno-unused-function)
probe_suffix=''
if [[ "${1:-}" == '--sanitize' ]]; then
    probe_flags=(-std=c99 -O1 -g -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined -fno-omit-frame-pointer)
    probe_suffix='_sanitized'
elif [[ "$#" -ne 0 ]]; then
    echo 'Usage: bash experiments/run_probes.sh [--sanitize]' >&2
    exit 1
fi
gcc "${probe_flags[@]}" experiments/processing_probe.c -lm -lpthread -ldl -o "work/processing_probe${probe_suffix}"
if [[ "$probe_suffix" == "_sanitized" ]]; then
    # LeakSanitizer cannot operate under the executor ptrace supervisor.
    ASAN_OPTIONS=detect_leaks=0 "./work/processing_probe${probe_suffix}" > "experiments/processing_probe${probe_suffix}_results.txt"
else
    "./work/processing_probe${probe_suffix}" > "experiments/processing_probe${probe_suffix}_results.txt"
fi
cat "experiments/processing_probe${probe_suffix}_results.txt"
