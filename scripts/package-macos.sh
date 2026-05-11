#!/usr/bin/env bash
# =============================================================================
# scripts/package-macos.sh
# Build a universal macOS DMG for Physics Simulation Studio.
#
# Usage:
#   ./scripts/package-macos.sh [--sign "Developer ID Application: Name (ID)"]
#
# Prerequisites:
#   - Xcode Command Line Tools
#   - Qt6 installed (e.g. via Homebrew: brew install qt@6)
#   - cmake and ninja (e.g. brew install cmake ninja)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build/macos-release"

# ── Parse args ───────────────────────────────────────────────────────────────
SIGN_IDENTITY=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --sign) SIGN_IDENTITY="$2"; shift 2;;
        *)      echo "Unknown arg: $1"; exit 1;;
    esac
done

echo "╔══════════════════════════════════════════════════╗"
echo "║   Physics Simulation Studio — macOS Packager     ║"
echo "╚══════════════════════════════════════════════════╝"

# ── Configure ─────────────────────────────────────────────────────────────────
echo ""
echo "▶ Configuring (Release, universal binary)..."
CMAKE_ARGS=(
    --preset macos-release
)
if [[ -n "$SIGN_IDENTITY" ]]; then
    CMAKE_ARGS+=("-DPSS_CODESIGN_IDENTITY=$SIGN_IDENTITY")
fi
cmake "${CMAKE_ARGS[@]}" -S "$ROOT_DIR" || cmake \
    -S "$ROOT_DIR" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    ${SIGN_IDENTITY:+-DPSS_CODESIGN_IDENTITY="$SIGN_IDENTITY"}

# ── Build ─────────────────────────────────────────────────────────────────────
echo ""
echo "▶ Building..."
cmake --build "$BUILD_DIR" --config Release --parallel "$(sysctl -n hw.logicalcpu)"

# ── Test ──────────────────────────────────────────────────────────────────────
echo ""
echo "▶ Running tests..."
(cd "$BUILD_DIR" && ctest --output-on-failure) || {
    echo "⚠️  Some tests failed — continuing with package step"
}

# ── Package ───────────────────────────────────────────────────────────────────
echo ""
echo "▶ Creating DMG..."
(cd "$BUILD_DIR" && cpack -G DragNDrop --verbose)

# ── Notarisation (optional) ───────────────────────────────────────────────────
DMG=$(find "$BUILD_DIR" -name "*.dmg" | head -1)
if [[ -n "$DMG" && -n "$SIGN_IDENTITY" ]]; then
    echo ""
    echo "▶ Submitting for Apple notarisation..."
    echo "  (Set APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD env vars)"
    if [[ -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" ]]; then
        xcrun notarytool submit "$DMG" \
            --apple-id    "$APPLE_ID" \
            --team-id     "$APPLE_TEAM_ID" \
            --password    "$APPLE_APP_PASSWORD" \
            --wait
        xcrun stapler staple "$DMG"
        echo "  ✅ Notarisation complete — DMG stapled"
    else
        echo "  ⚠️  Notarisation env vars not set — skipping notarisation"
    fi
fi

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
echo "✅ Package ready:"
find "$BUILD_DIR" -name "*.dmg" -print
