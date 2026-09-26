#!/usr/bin/env bash
# Linux installer for Google AI Studio Viewer (Ubuntu 24.04+)
#
# Build a Release binary, then stage an offline payload into
#   Installer/Linux/packages/com.googleaiviewer.app/data/
# ready for make-installer.sh (Qt Installer Framework -> *-offline.run).
#
# Usage:
#   Installer/Linux/linuxdeploy.sh [--qt-dir DIR] [--build-dir DIR] [--skip-build]
#
# Env:
#   QT_DIR                  Qt 6 prefix (e.g. $HOME/Qt/6.11.1/gcc_64).
#   GOOGLEAIVIEWER_VERSION  Overrides the version parsed from CMakeLists.txt
#                           (release jobs set this from the git tag).
#   GOOGLEAIVIEWER_SKIP_BUILD=1  Reuse the existing build instead of rebuilding.
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

while [ $# -gt 0 ]; do
    case "$1" in
        --qt-dir) QT_DIR_ARG="$2"; shift 2 ;;
        --build-dir) BUILD_DIR_ARG="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# --- Resolve Qt ---
QT_DIR="${QT_DIR_ARG:-${QT_DIR:-}}"
if [ -z "$QT_DIR" ]; then
    for cand in "$HOME/Qt/6.11.1/gcc_64" "$HOME/Qt/6.10.1/gcc_64" "/opt/Qt/6.11.1/gcc_64"; do
        if [ -x "$cand/bin/qmake" ]; then QT_DIR="$cand"; break; fi
    done
fi
if [ -z "$QT_DIR" ] || [ ! -x "$QT_DIR/bin/qmake" ]; then
    echo "ERROR: Qt not found. Set QT_DIR (e.g. export QT_DIR=\$HOME/Qt/6.11.1/gcc_64)." >&2
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
    GENERATOR="Unix Makefiles"
    command -v ninja >/dev/null 2>&1 && GENERATOR="Ninja"
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
        -DCMAKE_PREFIX_PATH="$QT_DIR" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --config Release -j"$(nproc)"
else
    echo "Skipping build (GOOGLEAIVIEWER_SKIP_BUILD=1)."
fi

BIN="$BUILD_DIR/$APP_NAME"
if [ ! -x "$BIN" ]; then
    echo "ERROR: binary not found at $BIN. Build first or pass --build-dir." >&2
    exit 1
fi

# --- Stage data/ ---
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR/lib" "$DATA_DIR/plugins"

cp "$BIN" "$DATA_DIR/$APP_NAME.bin"
cp "$SCRIPT_DIR/googleaiviewer.sh" "$DATA_DIR/googleaiviewer.sh"
cp "$SCRIPT_DIR/googleaiviewer.desktop" "$DATA_DIR/googleaiviewer.desktop"
cp "$REPO_ROOT/icon_512.png" "$DATA_DIR/googleaiviewer.png"
chmod +x "$DATA_DIR/googleaiviewer.sh"

cat > "$DATA_DIR/qt.conf" <<'EOF'
[Paths]
Prefix=.
Libraries=lib
Plugins=plugins
EOF

# --- Bundle Qt libraries: ldd closure over the binary AND every staged
# --- plugin, copying only libQt6*/libicu*/libKF6*. Never bundle system libs
# --- (xcb, GL, fontconfig) -- the launcher preflights those instead.
# --- NOTE: ldd reports "$ORIGIN"-unresolvable Qt deps of freshly staged
# --- plugins as "=> not found" (system ld cache knows nothing of $QT_DIR),
# --- so those are resolved against $QT_LIB_DIR explicitly.
QT_LIB_DIR="$QT_DIR/lib"
copy_needed_libs() {
    local file="$1"
    local line lib target src base
    while IFS= read -r line; do
        lib="$(echo "$line" | awk '{print $1}')"
        case "$lib" in
            libQt6*|libicu*|libKF6*) ;;
            *) continue ;;
        esac
        base="$(basename "$lib")"
        if [ -f "$DATA_DIR/lib/$base" ]; then continue; fi
        target="$(echo "$line" | grep -o '/[^ ]*' || true)"
        if [ -n "$target" ] && [ -f "$target" ]; then
            src="$target"
        elif [ -f "$QT_LIB_DIR/$base" ]; then
            src="$QT_LIB_DIR/$base"
        else
            echo "WARNING: cannot resolve $base (needed by $file)" >&2
            continue
        fi
        cp -L "$src" "$DATA_DIR/lib/$base"
        echo "bundled: $base"
    done < <(ldd "$file" 2>/dev/null)
}

copy_needed_libs "$DATA_DIR/$APP_NAME.bin"

# --- Copy the needed plugins/ subsets from the Qt prefix ---
PLUGIN_SUBSETS="platforms imageformats sqldrivers tls networkinformation iconengines platforminputcontexts xcbglintegrations wayland-shell-integration wayland-graphics-integration-client wayland-decoration-client generic"
for subset in $PLUGIN_SUBSETS; do
    if [ -d "$QT_DIR/plugins/$subset" ]; then
        mkdir -p "$DATA_DIR/plugins/$subset"
        for plug in "$QT_DIR/plugins/$subset"/libq*.so; do
            [ -e "$plug" ] || continue
            case "$(basename "$plug")" in
                # Skip database drivers and image formats the app never uses.
                libqsqlmysql.so|libqsqlodbc.so|libqsqlpsql.so|libqsqlibase.so|libqsqlmimer.so|libqsqloci.so) continue ;;
            esac
            cp -L "$plug" "$DATA_DIR/plugins/$subset/"
        done
    fi
done

# Repeat the ldd closure over the binary, every staged plugin, and every
# staged lib until no new libs appear (second-order plugin deps such as
# libQt6XcbQpa/libQt6Svg resolve via $QT_LIB_DIR on the first pass).
for _pass in 1 2 3 4 5; do
    before="$(ls "$DATA_DIR/lib" | wc -l)"
    copy_needed_libs "$DATA_DIR/$APP_NAME.bin"
    while IFS= read -r -d '' plug; do
        copy_needed_libs "$plug"
    done < <(find "$DATA_DIR/plugins" "$DATA_DIR/lib" -type f -name '*.so*' -print0)
    after="$(ls "$DATA_DIR/lib" | wc -l)"
    if [ "$after" -eq "$before" ]; then break; fi
done

if ! ls "$DATA_DIR/lib"/libQt6Core.so* >/dev/null 2>&1; then
    echo "ERROR: no Qt libs staged -- is the binary linked against \$QT_DIR?" >&2
    exit 1
fi

# --- Fix rpaths so the bundle is relocatable ---
command -v patchelf >/dev/null 2>&1 || { echo "ERROR: patchelf not found (sudo apt install patchelf)." >&2; exit 1; }
patchelf --set-rpath '$ORIGIN/lib' "$DATA_DIR/$APP_NAME.bin"
for lib in "$DATA_DIR"/lib/*.so*; do
    [ -f "$lib" ] || continue
    [ -L "$lib" ] && continue
    patchelf --set-rpath '$ORIGIN' "$lib" || true
done
while IFS= read -r -d '' plug; do
    patchelf --set-rpath '$ORIGIN/../../lib' "$plug" || true
done < <(find "$DATA_DIR/plugins" -name '*.so' -print0)

echo "Staged payload in $DATA_DIR"
echo "Next: Installer/Linux/make-installer.sh"
