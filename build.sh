#!/usr/bin/env bash
# ./build.sh        generate headers from the sheets, build and run the core tests
# ./build.sh asi    additionally cross-build build/RewindTime.asi (needs a Windows
#                   cross compiler: mingw-w64 or zig)
set -euo pipefail
cd "$(dirname "$0")"

python3 tools/gen.py
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -Isrc tests/test_core.cpp -o build/test_core
./build/test_core

if [ "${1:-}" = "asi" ]; then
  if command -v x86_64-w64-mingw32-g++ >/dev/null; then
    x86_64-w64-mingw32-g++ -std=c++17 -O2 -shared -static -Isrc src/plugin.cpp -o build/RewindTime.dll
  elif python3 -m ziglang version >/dev/null 2>&1; then
    python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++17 -O2 -shared -Isrc src/plugin.cpp -o build/RewindTime.dll
  else
    echo "No Windows cross compiler found (mingw-w64 or zig)." >&2
    exit 2
  fi
  mv -f build/RewindTime.dll build/RewindTime.asi
  echo "Built build/RewindTime.asi (source only: not tested in the game)"
fi
