#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# One-time package setup on Debian/Ubuntu:
#   ./scripts/ci/install_linux_deps.sh
# Optional local dependency checkout:
#   export JUCE_DIR=/path/to/JUCE
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
printf '\nBuilt artifacts are under:\n%s\n' "$ROOT/build/linux-release/DEVICEControl_artefacts"
