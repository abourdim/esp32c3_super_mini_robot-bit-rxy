#!/usr/bin/env bash
# ota_flash.sh — push firmware to the robot over WiFi OTA
#
# Usage: ./ota_flash.sh <robot-ip>
#
# The robot must already be in OTA mode: hold the debug button for
# CONFIG_OTA_HOLD_MS (default 3s) during normal operation, then check the
# serial monitor for a line like:
#   [OTA] WiFi connected, IP: 192.168.1.187

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

if [ -z "${1:-}" ]; then
    echo "Usage: $0 <robot-ip>"
    echo
    echo "Get the IP from the robot's serial monitor after holding the"
    echo "debug button for ~3s: '[OTA] WiFi connected, IP: ...'"
    exit 1
fi

ROBOT_IP="$1"

echo "Flashing $ROBOT_IP over WiFi OTA..."
pio run -e esp32-c3-devkitm-1-ota -t upload --upload-port "$ROBOT_IP"
