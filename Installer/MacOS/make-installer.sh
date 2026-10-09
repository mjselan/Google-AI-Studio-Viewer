#!/usr/bin/env bash
# Build the macOS installer for Google AI Studio Viewer.
#
# Default: create a drag-and-drop DMG with hdiutil (no IFW needed).
# --ifw:    create an IFW wizard DMG via binarycreator.
#
# Output: Installer/MacOS/GoogleAIViewer-<version>.dmg
#
# Env:
#   IFW_BIN               Explicit path to binarycreator (file) or its dir.
#   QT_DIR                Qt prefix; used to find a sibling Tools/QtInstallerFramework.
#   GOOGLEAIVIEWER_VERSION  Overrides the version parsed from CMakeLists.txt.
#   DMG_VOLUME_NAME       Volume name for the DMG (default: "Google AI Studio Viewer").
#   DMG_WINDOW_SIZE       Window size for the DMG (default: "600 400").
#
# Adapted from installers.md (Qt 6 Widgets offline-installer recipe).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPONENT_ID="com.googleaiviewer.app"
APP_NAME="GoogleAIViewer"
DATA_DIR="$SCRIPT_DIR/packages/$COMPONENT_ID/data"

USE_IFW=0
while [ $# -gt 0 ]; do
    case "$1" in
        --ifw) USE_IFW=1; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# --- Resolve version ---
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

# --- Validate staged payload ---
APP_BUNDLE="$DATA_DIR/$APP_NAME.app"
if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: staged payload missing. Run macdeploy.sh first." >&2
    exit 1
fi

# --- Stamp the real version into IFW metadata (used by the --ifw path) ---------
for _f in "$SCRIPT_DIR/config/config.xml" "$SCRIPT_DIR/packages/$COMPONENT_ID/meta/package.xml"; do
    sed -i'' "s|<Version>[^<]*</Version>|<Version>$VERSION</Version>|" "$_f"
done
sed -i'' "s|<ReleaseDate>[^<]*</ReleaseDate>|<ReleaseDate>$(date +%F)</ReleaseDate>|" \
    "$SCRIPT_DIR/packages/$COMPONENT_ID/meta/package.xml"
echo "Stamped installer metadata version: $VERSION"

DMG_OUT="$SCRIPT_DIR/GoogleAIViewer-$VERSION.dmg"
VOLUME_NAME="${DMG_VOLUME_NAME:-Google AI Studio Viewer}"

# --- hdiutil is flaky on shared CI runners ("Resource busy") when a previous
# --- attempt left /Volumes/<name> mounted. Detach stale mounts before/after,
# --- retry a few times. Do NOT kill Finder or wipe /tmp globally.
detach_stale_dmg() {
    local vol="/Volumes/$VOLUME_NAME"
    if [ -e "$vol" ]; then
        hdiutil detach "$vol" -force 2>/dev/null \
            || diskutil unmount force "$vol" 2>/dev/null \
            || true
    fi
}

create_dmg() {
    local src="$1"
    local attempt=1 max_attempts=3
    while [ "$attempt" -le "$max_attempts" ]; do
        detach_stale_dmg
        rm -f "$DMG_OUT"
        if hdiutil create -volname "$VOLUME_NAME" \
            -srcfolder "$src" \
            -ov -format UDZO \
            "$DMG_OUT"; then
            detach_stale_dmg
            return 0
        fi
        echo "hdiutil create failed (attempt $attempt/$max_attempts), retrying..." >&2
        sleep 5
        attempt=$((attempt + 1))
    done
    echo "ERROR: hdiutil create failed after $max_attempts attempts." >&2
    return 1
}

if [ "$USE_IFW" -eq 1 ]; then
    # --- IFW wizard DMG ---
    echo "Building IFW wizard DMG..."

    # Resolve binarycreator (same logic as Linux make-installer.sh)
    resolve_ifw() {
        if [ -n "${IFW_BIN:-}" ]; then
            if [ -x "$IFW_BIN" ]; then echo "$IFW_BIN"; return 0; fi
            if [ -x "$IFW_BIN/binarycreator" ]; then echo "$IFW_BIN/binarycreator"; return 0; fi
        fi
        local roots=()
        if [ -n "${QT_DIR:-}" ]; then roots+=("$(cd "$QT_DIR/.." 2>/dev/null && cd ../Tools/QtInstallerFramework 2>/dev/null && pwd)"); fi
        roots+=("$HOME/Qt/Tools/QtInstallerFramework" "/opt/Qt/Tools/QtInstallerFramework")
        local best="" best_major=0 best_minor=0 best_patch=0
        for root in "${roots[@]}"; do
            [ -d "$root" ] || continue
            for d in "$root"/*/; do
                [ -d "$d" ] || continue
                local v
                v="$(basename "$d")"
                if [[ "$v" =~ ^([0-9]+)\.([0-9]+)(\.([0-9]+))?$ ]]; then
                    local major="${BASH_REMATCH[1]}" minor="${BASH_REMATCH[2]}" patch="${BASH_REMATCH[4]:-0}"
                    if [ "$major" -ne 4 ]; then continue; fi
                    if [ "$major" -gt "$best_major" ] \
                        || { [ "$major" -eq "$best_major" ] && [ "$minor" -gt "$best_minor" ]; } \
                        || { [ "$major" -eq "$best_major" ] && [ "$minor" -eq "$best_minor" ] && [ "$patch" -gt "$best_patch" ]; }; then
                        if [ -x "$d/bin/binarycreator" ]; then
                            best="$d/bin/binarycreator"
                            best_major="$major"; best_minor="$minor"; best_patch="$patch"
                        fi
                    fi
                fi
            done
        done
        # Hard preference for 4.11 when present
        for root in "${roots[@]}"; do
            if [ -x "$root/4.11/bin/binarycreator" ]; then echo "$root/4.11/bin/binarycreator"; return 0; fi
        done
        if [ -n "$best" ]; then echo "$best"; return 0; fi
        if command -v binarycreator >/dev/null 2>&1; then command -v binarycreator; return 0; fi
        return 1
    }

    BINARYCREATOR="$(resolve_ifw)" || { echo "ERROR: binarycreator not found (set IFW_BIN)." >&2; exit 1; }
    echo "binarycreator: $BINARYCREATOR"

    # Build the IFW installer
    IFW_INSTALLER="$SCRIPT_DIR/GoogleAIViewer-$VERSION-offline"
    rm -f "$IFW_INSTALLER"
    "$BINARYCREATOR" --offline-only \
        -c "$SCRIPT_DIR/config/config.xml" \
        -p "$SCRIPT_DIR/packages" \
        "$IFW_INSTALLER"
    chmod +x "$IFW_INSTALLER"

    # Wrap the IFW installer in a DMG
    create_dmg "$IFW_INSTALLER"
    rm -f "$IFW_INSTALLER"

else
    # --- Drag-and-drop DMG (default) ---
    echo "Building drag-and-drop DMG..."

    WINDOW_SIZE="${DMG_WINDOW_SIZE:-600 400}"

    # Create a temporary directory for the DMG layout
    TMP_DIR="$(mktemp -d)"
    trap 'rm -rf "$TMP_DIR"' EXIT

    # Copy the .app and a symlink to /Applications
    cp -R "$APP_BUNDLE" "$TMP_DIR/"
    ln -s /Applications "$TMP_DIR/Applications"

    # Create the DMG (with stale-mount detach + retry for busy CI runners)
    create_dmg "$TMP_DIR"

    # Set window layout (optional, for a polished look)
    # This is a nice-to-have; the DMG works without it
    if command -v applescript >/dev/null 2>&1 || command -v osascript >/dev/null 2>&1; then
        echo "DMG created. To customize the window layout, open it in Finder and run:"
        echo "  hdiutil attach $DMG_OUT"
        echo "  # Arrange windows, then:"
        echo "  hdiutil detach /Volumes/$VOLUME_NAME"
    fi
fi

echo "Wrote $DMG_OUT"
