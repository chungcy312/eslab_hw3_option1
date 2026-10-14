#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATTLIB_DIR="$PROJECT_DIR/gattlib-src"
INSTALL_DIR="$PROJECT_DIR/.local"
BUILD_DIR="$PROJECT_DIR/build"

sudo apt update
sudo apt install -y \
    build-essential cmake git pkg-config \
    libbluetooth-dev libdbus-1-dev libglib2.0-dev \
    libpcre2-dev libreadline-dev

if [[ ! -d "$GATTLIB_DIR/.git" ]]; then
    git clone --depth 1 https://github.com/labapart/gattlib.git "$GATTLIB_DIR"
fi

# Older upstream example CMake files request PCRE1. Raspberry Pi OS releases
# that provide PCRE2 only can build the examples after this mechanical update.
if grep -R -q 'REQUIRED libpcre)' "$GATTLIB_DIR/examples"; then
    grep -rl 'REQUIRED libpcre)' "$GATTLIB_DIR/examples" \
        | xargs -r sed -i 's/REQUIRED libpcre)/REQUIRED libpcre2-8)/'
fi

cmake -S "$GATTLIB_DIR" -B "$GATTLIB_DIR/build" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCPACK_PACKAGE_INSTALL_DIRECTORY="$INSTALL_DIR" \
    -DGATTLIB_LOG_BACKEND=printf
cmake --build "$GATTLIB_DIR/build" --parallel 2
cmake --install "$GATTLIB_DIR/build"

export PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig:$INSTALL_DIR/lib/aarch64-linux-gnu/pkgconfig:$INSTALL_DIR/lib/arm-linux-gnueabihf/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$INSTALL_DIR/lib/aarch64-linux-gnu:$INSTALL_DIR/lib/arm-linux-gnueabihf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 2

printf '\nBuild complete. Start the C BLE central with:\n'
printf '  LD_LIBRARY_PATH="%s" "%s/ble_indication_central" --char <CHARACTERISTIC_UUID>\n' \
    "$LD_LIBRARY_PATH" "$BUILD_DIR"
