#!/usr/bin/env bash
# macOS deploy for Google AI Studio Viewer
#
# Build a Release .app bundle, run macdeployqt to inline Qt libraries,
# then stage the result into:
#   Installer/MacOS/packages/com.googleaiviewer.app/data/
# ready for make-installer.sh (hdiutil DMG or IFW DMG).
#
# Usage:
#   Installer/MacOS/macdeploy.sh [--qt-dir DIR] [--build-dir DIR] [--skip-build]
#                                [--universal] [--codesign IDENTITY]
#
# Env:
#   QT_DIR                      Qt 6 prefix (e.g. $HOME/Qt/6.11.1/macos).
#   GOOGLEAIVIEWER_VERSION      Overrides the version parsed from CMakeLists.txt.
#   GOOGLEAIVIEWER_SKIP_BUILD=1 Reuse the existing build instead of rebuilding.
#   CODESIGN_IDENTITY           Ad-hoc signing identity (default: "-").
#
# Adapted from installers.md (Qt 6 Widgets offline-installer recipe).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPONENT_ID="com.googleaiviewer.app"
APP_NAME="GoogleAIViewer"
DATA_DIR="$SCRIPT_DIR/packages/$COMPONENT_ID/data"

QT_DIR_ARG=""
BUILD_DIR_ARG=""
SKIP_BUILD="${GOOGLEAIVIEWER_SKIP_BUILD:-0}"
UNIVERSAL=0
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:--}"

while [ $# -gt 0 ]; do
    case "$1" in
        --qt-dir) QT_DIR_ARG="$2"; shift 2 ;;
        --build-dir) BUILD_DIR_ARG="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --universal) UNIVERSAL=1; shift ;;
        --codesign) CODESIGN_IDENTITY="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# --- Resolve Qt ---
QT_DIR="${QT_DIR_ARG:-${QT_DIR:-}}"
if [ -z "$QT_DIR" ]; then
    for cand in "$HOME/Qt/6.11.1/macos" "$HOME/Qt/6.11.1/clang_64" "/usr/local/opt/qt@6" "/opt/homebrew/opt/qt@6"; do
        if [ -x "$cand/bin/qmake" ]; then QT_DIR="$cand"; break; fi
    done
fi
if [ -z "$QT_DIR" ] || [ ! -x "$QT_DIR/bin/qmake" ]; then
    echo "ERROR: Qt not found. Set QT_DIR (e.g. export QT_DIR=\$HOME/Qt/6.11.1/macos)." >&2
    exit 1
fi
echo "Qt: $QT_DIR"

# --- Resolve version: env override wins, else parse project(VERSION) ---
if [ -n "${GOOGLEAIVIEWER_VERSION:-}" ]; then
    VERSION="$GOOGLEAIVIEWER_VERSION"
else
    VERSION="$(grep -E 'project\(GoogleAIViewer VERSION [0-9]+\.[0-9]+\.[0-9]+' "$REPO_ROOT/CMakeLists.txt" \
        | sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/')"
fi
if [ -z "${VERSION:-}" ]; then
    echo "ERROR: could not determine version (set GOOGLEAIVIEWER_VERSION)." >&2
    exit 1
fi
echo "Version: $VERSION"

# --- Build Release ---
BUILD_DIR="${BUILD_DIR_ARG:-$REPO_ROOT/build}"
if [ "$SKIP_BUILD" != "1" ]; then
    CMAKE_ARGS=(-S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja
        -DCMAKE_PREFIX_PATH="$QT_DIR"
        -DCMAKE_BUILD_TYPE=Release)
    if [ "$UNIVERSAL" -eq 1 ]; then
        CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
    fi
    cmake "${CMAKE_ARGS[@]}"
    cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"
else
    echo "Skipping build (GOOGLEAIVIEWER_SKIP_BUILD=1)."
fi

APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: .app bundle not found at $APP_BUNDLE. Build first or pass --build-dir." >&2
    exit 1
fi

# --- Stage data/ ---
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"

# Copy the .app into the payload
ditto "$APP_BUNDLE" "$DATA_DIR/$APP_NAME.app"

# --- Run macdeployqt to inline Qt libraries ---
MACDEPLOYQT="$QT_DIR/bin/macdeployqt"
if [ ! -x "$MACDEPLOYQT" ]; then
    echo "ERROR: macdeployqt not found at $MACDEPLOYQT." >&2
    exit 1
fi

echo "Running macdeployqt..."
"$MACDEPLOYQT" "$DATA_DIR/$APP_NAME.app" -always-overwrite -codesign="$CODESIGN_IDENTITY"

# --- Validate the staged bundle ---
echo "Validating staged bundle..."

# Check for build-tree paths in linked libraries
if otool -L "$DATA_DIR/$APP_NAME.app/Contents/MacOS/$APP_NAME" | grep -q "$BUILD_DIR"; then
    echo "WARNING: binary references build-tree paths; macdeployqt may have missed something." >&2
fi

# Validate Info.plist
if [ -f "$DATA_DIR/$APP_NAME.app/Contents/Info.plist" ]; then
    plutil -lint "$DATA_DIR/$APP_NAME.app/Contents/Info.plist"
fi

# Verify code signature
if [ "$CODESIGN_IDENTITY" != "-" ]; then
    codesign --verify --deep --strict "$DATA_DIR/$APP_NAME.app" || {
        echo "WARNING: codesign verification failed." >&2
    }
else
    echo "Ad-hoc signing only; skipping deep verification."
fi

echo "Staged payload in $DATA_DIR"
echo "Next: Installer/MacOS/make-installer.sh"
