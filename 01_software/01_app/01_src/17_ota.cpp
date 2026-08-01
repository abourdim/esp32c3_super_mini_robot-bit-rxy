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
//
// Feedback is on both the NeoPixels (color/blink, readable from across a
// room) and the OLED (exact text — IP address, percent complete — that a
// color alone can't convey).
// ===========================================================================

extern Adafruit_SSD1306 display;

static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// Two-line status screen, big enough to read at a glance. Kept separate
// from oled_update()'s normal driving readout — OTA mode never returns to
// loop(), so the two never run concurrently.
static void otaShowStatus(const String& line1, const String& line2 = "") {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(line1);
  if (line2.length()) {
    display.setTextSize(1);
    display.setCursor(0, 24);
    display.print(line2);
  }
  display.display();
}

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
  otaShowStatus("OTA mode", "Connecting WiFi...");

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
    otaShowStatus("WiFi failed", "Rebooting...");
    ESP.restart();
  }

  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("\n[OTA] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  #endif
  neopixels_all_blink(CRGB::Green, 2, 100);
  otaShowStatus("OTA ready", WiFi.localIP().toString());

  ArduinoOTA.setHostname(CONFIG_OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("[OTA] Update starting");
    #endif
    neopixels_all_clear(CRGB::Black);
    otaShowStatus("Uploading...", "0%");
  });
  ArduinoOTA.onEnd([]() {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("\n[OTA] Update complete — rebooting");
    #endif
    neopixels_all_blink(CRGB::Green, 4, 80);
    otaShowStatus("Done!", "Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Light one more pixel as progress advances, wrapping if there are
    // fewer LEDs than progress steps — just a visual heartbeat, not a
    // precise gauge.
    uint8_t idx = (progress * CONFIG_NEOPIXELS_NB_LEDS / total) % CONFIG_NEOPIXELS_NB_LEDS;
    neopixels_pulse(idx, CRGB::White);
    FastLED.show();

    // Only redraw the OLED when the whole percent changes — this fires many
    // times a second, and the I2C display write is slow enough to matter
    // if done on every single callback.
    static int8_t s_lastPct = -1;
    int8_t pct = (int8_t)((progress * 100UL) / total);
    if (pct != s_lastPct) {
      s_lastPct = pct;
      otaShowStatus("Uploading...", String(pct) + "%");
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[OTA] Error[%u]\n", error);
    #endif
    neopixels_all_blink(CRGB::Red, 5, 100);
    otaShowStatus("Error!", "code " + String((int)error));
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
