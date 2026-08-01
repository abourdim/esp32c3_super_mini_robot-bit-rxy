#include "01_includes.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

// ===========================================================================
// WiFi OTA — deliberately kept out of the robot's normal BLE-only operation.
// Entered by holding the debug button for CONFIG_OTA_HOLD_MS at any point
// while the robot is running (checked every loop() iteration, not just
// right after boot — GPIO0 is also the chip's BOOT strapping pin, so
// sampling it near reset is unreliable; checking during normal runtime
// avoids that entirely). Once entered, this never returns to loop(): the
// robot either gets reflashed or times out and reboots back into normal
// BLE mode.
// ===========================================================================

static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// Tracks an in-progress hold across successive loop() calls. Returns true
// exactly once, the instant the hold crosses CONFIG_OTA_HOLD_MS, so the
// caller triggers OTA entry a single time per hold rather than repeatedly.
static bool buttonHeldForOtaEntry() {
  static uint32_t s_pressStartMs = 0;
  static bool s_wasPressed = false;
  static bool s_triggeredThisHold = false;

  if (!button_pressed()) {
    s_wasPressed = false;
    s_triggeredThisHold = false;
    return false;
  }

  if (!s_wasPressed) {
    s_wasPressed = true;
    s_pressStartMs = millis();
    return false;
  }

  if (!s_triggeredThisHold && (millis() - s_pressStartMs >= CONFIG_OTA_HOLD_MS)) {
    s_triggeredThisHold = true;
    return true;
  }
  return false;
}

static void otaEnterAndBlock() {
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[OTA] Button held — entering OTA mode");
  #endif

  neopixels_all_blink(CRGB::Blue, 2, 100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(CONFIG_WIFI_OTA_SSID, CONFIG_WIFI_OTA_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    neopixels_pulse(0, CRGB::Blue);
    FastLED.show();
    delay(200);
    #ifdef DEF_DERIAL_DEBUG
    Serial.print(".");
    #endif
  }

  if (WiFi.status() != WL_CONNECTED) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("\n[OTA] WiFi connect timed out — rebooting into normal mode");
    #endif
    neopixels_all_blink(CRGB::Red, 3, 150);
    ESP.restart();
  }

  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("\n[OTA] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  #endif
  neopixels_all_blink(CRGB::Green, 2, 100);

  ArduinoOTA.setHostname(CONFIG_OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("[OTA] Update starting");
    #endif
    neopixels_all_clear(CRGB::Black);
  });
  ArduinoOTA.onEnd([]() {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("\n[OTA] Update complete — rebooting");
    #endif
    neopixels_all_blink(CRGB::Green, 4, 80);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Light one more pixel as progress advances, wrapping if there are
    // fewer LEDs than progress steps — just a visual heartbeat, not a
    // precise gauge.
    uint8_t idx = (progress * CONFIG_NEOPIXELS_NB_LEDS / total) % CONFIG_NEOPIXELS_NB_LEDS;
    neopixels_pulse(idx, CRGB::White);
    FastLED.show();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[OTA] Error[%u]\n", error);
    #endif
    neopixels_all_blink(CRGB::Red, 5, 100);
  });

  ArduinoOTA.begin();

  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[OTA] Ready — waiting for firmware upload");
  #endif

  // Never returns: OTA mode replaces the normal robot loop entirely for
  // this power cycle.
  while (true) {
    ArduinoOTA.handle();
    delay(5);
  }
}

void ota_check_long_press() {
  if (buttonHeldForOtaEntry()) {
    otaEnterAndBlock();
  }
}
