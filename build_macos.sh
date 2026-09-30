#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# Optional: export JUCE_DIR=/path/to/JUCE to use a local checkout.
# Without JUCE_DIR, the pinned JUCE release is fetched automatically.
cmake --preset macos-release
cmake --build --preset macos-release --parallel
ctest --preset macos-release
printf '\nBuilt artifacts are under:\n%s\n' "$ROOT/build/macos-release/DEVICEControl_artefacts"
