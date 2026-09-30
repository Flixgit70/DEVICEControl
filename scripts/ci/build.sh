#!/usr/bin/env bash
set -euo pipefail

PRESET="${1:-linux-release}"
case "$PRESET" in
  linux-release|macos-release) ;;
  *) echo "Unsupported Unix preset: $PRESET" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel
ctest --preset "$PRESET"
