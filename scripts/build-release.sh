#!/usr/bin/env bash
# =============================================================================
# scripts/build-release.sh
# One-shot release build + test + package for the current platform.
# Detects the OS and runs the appropriate preset automatically.
#
# Usage:
#   ./scripts/build-release.sh [--jobs N] [--no-tests] [--no-package]
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# ── Argument parsing ──────────────────────────────────────────────────────────
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
RUN_TESTS=1
RUN_PACKAGE=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --jobs)       JOBS="$2"; shift 2;;
        --no-tests)   RUN_TESTS=0;   shift;;
        --no-package) RUN_PACKAGE=0; shift;;
        -h|--help)
            echo "Usage: $0 [--jobs N] [--no-tests] [--no-package]"
            exit 0;;
        *) echo "Unknown option: $1"; exit 1;;
    esac
done

# ── Detect platform & select preset ──────────────────────────────────────────
case "$(uname -s)" in
    Darwin)  PRESET="macos-release"  ;;
    Linux)   PRESET="linux-release"  ;;
    MINGW*|CYGWIN*|MSYS*)
             PRESET="windows-msvc-release" ;;
    *)       echo "Unsupported platform: $(uname -s)"; exit 1;;
esac

BUILD_DIR="$ROOT_DIR/build/$PRESET"

echo "╔══════════════════════════════════════════════════════╗"
echo "║   Physics Simulation Studio — Release Build Script   ║"
echo "╚══════════════════════════════════════════════════════╝"
echo "  Platform : $(uname -s)"
echo "  Preset   : $PRESET"
echo "  Build dir: $BUILD_DIR"
echo "  Jobs     : $JOBS"
echo ""

# ── Configure ─────────────────────────────────────────────────────────────────
echo "▶ [1/4] Configure..."
cmake --preset "$PRESET" -S "$ROOT_DIR" || \
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DPSS_BUILD_TESTS=ON
echo "  ✓ Configure OK"

# ── Build ─────────────────────────────────────────────────────────────────────
echo ""
echo "▶ [2/4] Build..."
cmake --build "$BUILD_DIR" --config Release --parallel "$JOBS"
echo "  ✓ Build OK"

# ── Test ──────────────────────────────────────────────────────────────────────
if [[ $RUN_TESTS -eq 1 ]]; then
    echo ""
    echo "▶ [3/4] Tests..."
    (cd "$BUILD_DIR" && ctest --output-on-failure -C Release) && \
        echo "  ✓ All tests passed" || \
        echo "  ⚠ Some tests failed"
else
    echo ""
    echo "  [3/4] Tests skipped (--no-tests)"
fi

# ── Package ───────────────────────────────────────────────────────────────────
if [[ $RUN_PACKAGE -eq 1 ]]; then
    echo ""
    echo "▶ [4/4] Package..."
    (cd "$BUILD_DIR" && cpack --verbose)
    echo ""
    echo "  ✓ Packages:"
    find "$BUILD_DIR" \
        \( -name "*.dmg" -o -name "*.exe" -o -name "*.tar.gz" -o -name "*.zip" -o -name "*.deb" \) \
        -newer "$BUILD_DIR/CMakeCache.txt" \
        -print | sed 's/^/    /'
else
    echo ""
    echo "  [4/4] Package skipped (--no-package)"
fi

echo ""
echo "✅ Done!"
