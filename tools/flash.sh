#!/usr/bin/env bash
# Usage:
#   ./flash.sh full <app_address>  - sign + flash bootloader + flash app
#   ./flash.sh app  <app_address>  - sign + flash app only
#
# Example:
#   ./flash.sh full 0x08008000
#   ./flash.sh app  0x08008000

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOTLOADER_BIN="$SCRIPT_DIR/../bootloader/Debug/bootloader.bin"
APP_BIN="$SCRIPT_DIR/../application/Debug/application.bin"
SIGN_SCRIPT="$SCRIPT_DIR/sign_firmware.py"
SIGNED_BIN="$SCRIPT_DIR/../application/Debug/application_signed.bin"
BOOTLOADER_ADDR="0x08000000"

info() {
    echo "[INFO]  $*"
}

sign_application() {
    local app_bin="$1"
    info "Signing application binary..."
    python3 "$SIGN_SCRIPT" sign --input "$app_bin" --version 1.2
}

flash_bootloader() {
    info "Flashing bootloader -> $BOOTLOADER_ADDR"
    st-flash --reset write "$BOOTLOADER_BIN" "$BOOTLOADER_ADDR"
    info "Bootloader flashed OK."
}

flash_application() {
    local addr="$1"
    info "Flashing signed application -> $addr"
    st-flash --reset write "$SIGNED_BIN" "$addr"
    info "Application flashed OK."
}

MODE="$1"
APP_ADDR="$2"

case "$MODE" in
    full)
        info "=== FULL FLASH: bootloader + application ==="
        sign_application "$APP_BIN"
        flash_bootloader
        flash_application "$APP_ADDR"
        info "=== Full flash complete ==="
        ;;
    app)
        info "=== APP FLASH: application only ==="
        sign_application "$APP_BIN"
        flash_application "$APP_ADDR"
        info "=== App flash complete ==="
        ;;
    *)
esac