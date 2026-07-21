#!/usr/bin/env bash
set -e

BUILD_DIR="build"
CONFIG="${1:-Debug}"
QT_PREFIX="${QT_DIR:-}"

# Auto-detect Qt if QT_DIR not set
if [ -z "$QT_PREFIX" ]; then
    if [ -d "$HOME/Qt/6.11.1/macos" ]; then
        QT_PREFIX="$HOME/Qt/6.11.1/macos"
    elif [ -d "$HOME/Qt/6.11.1/clang_64" ]; then
        QT_PREFIX="$HOME/Qt/6.11.1/clang_64"
    elif [ -d "/usr/local/opt/qt@6" ]; then
        QT_PREFIX="/usr/local/opt/qt@6"
    else
        echo "ERROR: Qt not found. Set QT_DIR to your Qt installation path."
        echo "  Example: export QT_DIR=/Users/you/Qt/6.11.1/macos"
        exit 1
    fi
fi

echo "Qt prefix: $QT_PREFIX"
echo "Build config: $CONFIG"

cmake -S . -B "$BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    -DCMAKE_BUILD_TYPE="$CONFIG"

cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
