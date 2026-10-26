#!/usr/bin/env bash
# Host-side test of the Qukeys tap-or-hold decision logic.
#
# Extracts the real processQueue(), releaseDelayed() and
# qukeyHeldThroughMinimumHoldTime() from QukeysDygma.cpp, compiles them
# against the stubs in sim.cpp, and checks timed key sequences against
# expected output with hold-on-other-key-press on and off.
#
# Usage: tools/qukeys-sim/run.sh [path/to/QukeysDygma.cpp]
# (defaults to the copy in the libraries submodule, with the patch applied)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
src=${1:-$here/../../libraries/kaleidoscope_adapter/plugins/QukeysDygma.cpp}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

extract() { awk -v pat="$2" '$0 ~ pat {p=1} p {print} p && /^}/ {p=0}' "$1"; }
{
  extract "$src" '^bool Qukeys::processQueue\\(\\)'
  extract "$src" '^bool Qukeys::releaseDelayed'
  extract "$src" '^bool Qukeys::qukeyHeldThroughMinimumHoldTime'
} > "$work/fns.inc"

if ! grep -q 'qukeyHeldThroughMinimumHoldTime() const' "$work/fns.inc"; then
  echo "error: $src does not contain the hold-on-other-key-press patch" >&2
  exit 1
fi

g++ -std=c++17 -Wall -Wno-unused-function -I"$work" -DFNS='"fns.inc"' "$here/sim.cpp" -o "$work/sim"
"$work/sim"
