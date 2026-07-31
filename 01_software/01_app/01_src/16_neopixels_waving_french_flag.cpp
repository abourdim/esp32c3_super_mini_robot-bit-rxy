#include "01_includes.h"

// Use config values — no local redefinitions
#define NUM_LEDS    CONFIG_NEOPIXELS_NB_LEDS
#define FRAMES_PER_SECOND 60

void neopixels_waving_french_flag_setup() {
  // FastLED already initialized in neopixels_init()
}

void neopixels_waving_french_flag_1() {
  static uint8_t wave = 0;
  wave += 2;

  for (int i = 0; i < NUM_LEDS; i++) {
    // proportional 3-section split (works with any LED count)
    uint8_t section = (i * 3) / NUM_LEDS;
    uint8_t brightness = 128 + sin8(wave + section * 85) / 2;

    switch (section) {
      case 0: leds[i] = CHSV(160, 255, brightness); break; // Blue
      case 1: leds[i] = CHSV(0, 0, brightness); break;      // White
      default: leds[i] = CHSV(0, 255, brightness); break;   // Red
    }
  }

  FastLED.show();
  delay(1000 / FRAMES_PER_SECOND);
}


#define TAIL_LENGTH 4          // Number of LEDs for each color trail

uint8_t offset = 0;

// French flag colors (prefixed to avoid shadowing FastLED built-ins)
const CRGB FLAG_BLUE(0, 0, 255);
const CRGB FLAG_WHITE(200, 200, 200);
const CRGB FLAG_RED(250, 0, 0);

uint8_t gHue = 0;




void neopixels_waving_french_flag_blocking() {
  
  
  static uint8_t wave = 0;
  wave += 2;  // Change for animation speed

  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t section = (i * 3) / NUM_LEDS;
    uint8_t brightness = 128 + sin8(wave + section * 85) / 2;

    switch (section) {
      case 0: leds[i] = CHSV(160, 255, brightness); break; // Blue
      case 1: leds[i] = CHSV(0, 0, brightness); break;      // White
      default: leds[i] = CHSV(0, 255, brightness); break;   // Red
    }
  }

  FastLED.show();
  delay(1000 / FRAMES_PER_SECOND);
}

void neopixels_waving_french_flag() {
  static uint8_t wave = 0;
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 1000 / FRAMES_PER_SECOND;  // en ms

  if (millis() - lastUpdate >= interval) {
    lastUpdate = millis();
    wave += 2;  // vitesse animation

    for (int i = 0; i < NUM_LEDS; i++) {
      // 3 sections proportionnelles
      uint8_t section = (i * 3) / NUM_LEDS;

      // onde décalée par LED + section
      uint8_t brightness = 128 + sin8(wave + i * 10 + section * 85) / 2;

      switch (section) {
        case 0: leds[i] = CHSV(160, 255, brightness); break; // Bleu
        case 1: leds[i] = CHSV(0, 0, brightness); break;     // Blanc
        case 2: leds[i] = CHSV(0, 255, brightness); break;   // Rouge
      }
    }

    FastLED.show();
  }
}