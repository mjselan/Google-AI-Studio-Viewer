#!/usr/bin/env bash
# Launcher for Google AI Studio Viewer (staged next to GoogleAIViewer.bin).
# Preflights system libraries we deliberately do NOT bundle, then execs the app.
set -e

APPDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$APPDIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$APPDIR/plugins"
export QT_QPA_PLATFORMTHEME=""

missing=()
for lib in libxcb.so.1 libGL.so.1 libfontconfig.so.1; do
    if ! ldconfig -p 2>/dev/null | grep -q "$lib"; then
        missing+=("$lib")
    fi
done

if [ "${#missing[@]}" -gt 0 ]; then
    echo "Google AI Studio Viewer: missing system libraries: ${missing[*]}" >&2
    echo "Install them with:" >&2
    echo "  sudo apt install libxcb-cursor0 libgl1 libfontconfig1" >&2
fi

exec "$APPDIR/GoogleAIViewer.bin" "$@"
