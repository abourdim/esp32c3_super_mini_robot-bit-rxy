#!/usr/bin/env bash
# ota_flash.sh — push firmware to the robot over WiFi OTA
#
# Usage: ./ota_flash.sh <robot-ip> [path/to/firmware.bin]
#
#   No image given   -> builds from current source and uploads (pio run -t upload)
#   Image path given -> uploads that exact .bin directly, no rebuild
#                        (e.g. a previously built .pio/build/*/firmware.bin,
#                        or one downloaded from a GitHub Actions build)
#
# The robot must already be in OTA mode: hold the debug button for
# CONFIG_OTA_HOLD_MS (default 3s) during normal operation, then check the
# serial monitor for a line like:
#   [OTA] WiFi connected, IP: 192.168.1.187

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

if [ -z "${1:-}" ]; then
    echo "Usage: $0 <robot-ip> [path/to/firmware.bin]"
    echo
    echo "Get the IP from the robot's serial monitor after holding the"
    echo "debug button for ~3s: '[OTA] WiFi connected, IP: ...'"
    exit 1
fi

ROBOT_IP="$1"
IMAGE="${2:-}"

if [ -z "$IMAGE" ]; then
    echo "Building from source and flashing $ROBOT_IP over WiFi OTA..."
    exec pio run -e esp32-c3-devkitm-1-ota -t upload --upload-port "$ROBOT_IP"
fi

if [ ! -f "$IMAGE" ]; then
    echo "[ERR] Image not found: $IMAGE" >&2
    exit 1
fi

ESPOTA="$HOME/.platformio/packages/framework-arduinoespressif32/tools/espota.py"
if [ ! -f "$ESPOTA" ]; then
    echo "[ERR] espota.py not found at $ESPOTA — is the espressif32 platform installed?" >&2
    exit 1
fi

echo "Flashing $IMAGE to $ROBOT_IP over WiFi OTA (no rebuild)..."
python "$ESPOTA" -i "$ROBOT_IP" -f "$IMAGE" -r
