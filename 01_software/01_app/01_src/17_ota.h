#ifndef __DEF_INCLUDE_OTA_H__
#define __DEF_INCLUDE_OTA_H__

// Call once from setup(), after button_init(). Polls the debug button for
// CONFIG_OTA_HOLD_MS; if held the whole time, connects to WiFi, starts
// ArduinoOTA, and blocks forever servicing OTA updates (never returns).
// If not held, returns immediately so the normal robot boot continues.
void ota_maybe_enter();

#endif // __DEF_INCLUDE_OTA_H__
