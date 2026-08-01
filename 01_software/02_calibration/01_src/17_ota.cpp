#include "01_includes.h"

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>

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
// No SSID/password compiled in. WiFiManager remembers the last network it
// joined (its own NVS storage, survives reflashes) and tries that first;
// if it can't connect, it opens its own "WDIY-Robot-Setup" access point —
// join it from a phone, a captive-portal page pops up to pick your real
// network and enter its password, which WiFiManager then saves for next
// time. Changing networks later never needs a recompile or USB reflash —
// hold the button past CONFIG_OTA_FORGET_HOLD_MS (8s total) instead of
// just CONFIG_OTA_HOLD_MS (3s) to forget the saved network and force the
// setup portal open even though a working one is already saved.
//
// Feedback is on both the NeoPixels (color/blink, readable from across a
// room) and the OLED (exact text — setup AP name, IP address, percent
// complete — that a color alone can't convey).
// ===========================================================================

extern Adafruit_SSD1306 display;

// Two-line status screen, big enough to read at a glance. Kept separate
// from oled_update()'s normal driving readout — OTA mode never returns to
// loop(), so the two never run concurrently.
static void otaShowStatus(const String& line1, const String& line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.print("Workshop-DIY");
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

// Called right as OTA mode is entered (button has already been held
// CONFIG_OTA_HOLD_MS at this point). Keeps polling the same hold for the
// remaining window up to CONFIG_OTA_FORGET_HOLD_MS; release early to skip.
static bool buttonHeldForForget() {
  uint32_t remaining = CONFIG_OTA_FORGET_HOLD_MS - CONFIG_OTA_HOLD_MS;
  uint32_t start = millis();
  while (millis() - start < remaining) {
    if (!button_pressed()) return false;
    delay(20);
  }
  return true;
}

static void otaEnterAndBlock() {
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[OTA] Button held — entering OTA mode");
  #endif

  neopixels_all_blink(CRGB::Blue, 2, 100);
  otaShowStatus("OTA mode", "Hold to forget WiFi...");

  WiFiManager wm;
  wm.setConfigPortalTimeout(CONFIG_OTA_PORTAL_TIMEOUT_S);

  if (buttonHeldForForget()) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("[OTA] Button held through forget window — clearing saved WiFi");
    #endif
    wm.resetSettings();
    neopixels_all_blink(CRGB::White, 2, 150);
    otaShowStatus("WiFi forgotten", "Opening setup...");
  } else {
    otaShowStatus("OTA mode", "Connecting WiFi...");
  }

  // Fires only if there's no saved network or it can't be reached — i.e.
  // the setup portal is about to open. Told apart from "just connecting to
  // the usual network" so the OLED/NeoPixels can say which one is happening.
  wm.setAPCallback([](WiFiManager* mgr) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[OTA] No saved WiFi — setup AP '%s' at %s\n",
                   CONFIG_OTA_SETUP_AP_NAME, WiFi.softAPIP().toString().c_str());
    #endif
    neopixels_all_blink(CRGB::Purple, 3, 150);
    otaShowStatus("Join WiFi:", CONFIG_OTA_SETUP_AP_NAME);
  });

  bool connected = wm.autoConnect(CONFIG_OTA_SETUP_AP_NAME);

  if (!connected) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println("[OTA] Setup portal timed out — rebooting into normal mode");
    #endif
    neopixels_all_blink(CRGB::Red, 3, 150);
    otaShowStatus("Setup timed out", "Rebooting...");
    ESP.restart();
  }

  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[OTA] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
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
