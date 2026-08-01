#include "01_includes.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

// ===========================================================================
// WiFi OTA — deliberately kept out of the robot's normal BLE-only boot path.
// Entered only if the debug button is held for CONFIG_OTA_HOLD_MS right
// after setup() starts (never during reset itself — GPIO0 is the chip's
// BOOT strapping pin, so it can't be safely sampled that early). Once
// entered, this never returns to loop(): the robot either gets reflashed
// or times out and reboots back into normal BLE mode.
// ===========================================================================

static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

static bool buttonHeldForOtaEntry() {
  uint32_t start = millis();
  while (millis() - start < CONFIG_OTA_HOLD_MS) {
    if (!button_pressed()) return false;
    delay(20);
  }
  return true;
}

static void otaEnterAndBlock() {
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[OTA] Button held at boot — entering OTA mode");
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

void ota_maybe_enter() {
  if (buttonHeldForOtaEntry()) {
    otaEnterAndBlock();
  }
}
