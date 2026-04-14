#!/usr/bin/env bash
# Thin wrapper around CMake. Defaults to a Release build in ./build/.
#
# Usage:
#   ./build.sh            # configure + build renderer_blueprint
#   ./build.sh demo       # also build the offline TGA demo
#   ./build.sh clean      # wipe the build dir
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# If a local MinGW toolchain is present on Windows, put it first. The bundled
# SDL2 under external/ is a MinGW import library, so we must build with MinGW
# rather than MSVC when we fall back to the bundled copy.
GEN_ARGS=()
if [[ -d "/c/mingw64/mingw64/bin" ]]; then
    export PATH="/c/mingw64/mingw64/bin:$PATH"
    GEN_ARGS=(-G "MinGW Makefiles")
fi

# Use D:/tmp for compiler temp files when C: is low on space
if [[ -d "/d" ]]; then
    mkdir -p /d/tmp
    export TMPDIR=/d/tmp TMP=/d/tmp TEMP=/d/tmp
fi

case "${1:-}" in
    clean)
        rm -rf build
        echo "[ok] build/ removed"
        exit 0
        ;;
    demo)
        TARGETS="renderer_blueprint renderer_demo"
        ;;
    *)
        TARGETS="renderer_blueprint"
        ;;
esac

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "${GEN_ARGS[@]}"
cmake --build build --config Release --target $TARGETS -j

echo
echo "[ok] Build complete. Binaries in build/"
ls -1 build/*.exe 2>/dev/null || ls -1 build/renderer_* 2>/dev/null || true
