#ifndef __DEF_INCLUDE_OTA_H__
#define __DEF_INCLUDE_OTA_H__

// Call every loop() iteration. Tracks the debug button across calls; once
// it's been held continuously for CONFIG_OTA_HOLD_MS, connects to WiFi,
// starts ArduinoOTA, and blocks forever servicing OTA updates (never
// returns to loop()). Returns immediately otherwise.
void ota_check_long_press();

#endif // __DEF_INCLUDE_OTA_H__
