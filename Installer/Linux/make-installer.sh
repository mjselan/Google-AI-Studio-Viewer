#!/usr/bin/env bash
# Build the offline Linux installer via Qt Installer Framework's binarycreator.
# Output: Installer/Linux/GoogleAIViewer-<version>-offline.run
#
# Env:
#   IFW_BIN               Explicit path to binarycreator (file) or its dir.
#   QT_DIR                Qt prefix; used to find a sibling Tools/QtInstallerFramework.
#   GOOGLEAIVIEWER_VERSION  Overrides the version parsed from CMakeLists.txt.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

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

DATA_DIR="$SCRIPT_DIR/packages/com.googleaiviewer.app/data"
if [ ! -x "$DATA_DIR/GoogleAIViewer.bin" ]; then
    echo "ERROR: staged payload missing. Run linuxdeploy.sh first." >&2
    exit 1
fi

# --- Resolve binarycreator: IFW_BIN -> sibling Tools/QtInstallerFramework of
# --- QT_DIR -> ~/Qt -> /opt/Qt -> PATH. Prefer 4.11, else numerically newest.
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
    # Hard preference for 4.11 when present (known-good with this recipe).
    for root in "${roots[@]}"; do
        if [ -x "$root/4.11/bin/binarycreator" ]; then echo "$root/4.11/bin/binarycreator"; return 0; fi
    done
    if [ -n "$best" ]; then echo "$best"; return 0; fi
    if command -v binarycreator >/dev/null 2>&1; then command -v binarycreator; return 0; fi
    return 1
}

BINARYCREATOR="$(resolve_ifw)" || { echo "ERROR: binarycreator not found (set IFW_BIN)." >&2; exit 1; }
echo "binarycreator: $BINARYCREATOR"

OUT="$SCRIPT_DIR/GoogleAIViewer-$VERSION-offline.run"
rm -f "$OUT"
"$BINARYCREATOR" --offline-only \
    -c "$SCRIPT_DIR/config/config.xml" \
    -p "$SCRIPT_DIR/packages" \
    "$OUT"
chmod +x "$OUT"
echo "Wrote $OUT"

# --- Smoke-test the staged app headless ---
echo "Smoke-testing staged payload (offscreen)..."
if QT_QPA_PLATFORM=offscreen timeout 10s "$DATA_DIR/googleaiviewer.sh" >/tmp/gaiv-smoke.log 2>&1; then
    echo "Smoke test exited 0."
else
    rc=$?
    if [ "$rc" -eq 124 ]; then
        echo "Smoke test still running after 10s (timeout) -- payload launches OK."
    else
        echo "Smoke test exited with code $rc; last lines:" >&2
        tail -20 /tmp/gaiv-smoke.log >&2 || true
        exit "$rc"
    fi
fi
