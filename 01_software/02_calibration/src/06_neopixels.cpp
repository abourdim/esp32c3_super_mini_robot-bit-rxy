#include "01_includes.h"

CRGB leds[CONFIG_NEOPIXELS_NB_LEDS];     // LED array

// ===========================================================================
void neopixels_init() {
// ===========================================================================  
  FastLED.addLeds<LED_TYPE, CONFIG_PIN_NEOPIXEL, COLOR_ORDER>(leds, CONFIG_NEOPIXELS_NB_LEDS);
  FastLED.setBrightness(CONFIG_NEOPIXELS_BRIGHTNESS);  // Set brightness (0-255)
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("FastLED NeoPixel example started");
  #endif
}

// ===========================================================================
void neopixels_rainbow(uint8_t ledIndex) {
// ===========================================================================
  static uint8_t hue = 0;
  leds[ledIndex] = CHSV(hue++, 255, 255);
  if(hue >= 255) hue = 0;
}

// ===========================================================================
void neopixels_pulse(uint8_t ledIndex, CRGB color) {
// ===========================================================================
  static uint8_t brightness = 0;
  static bool increasing = true;
  
  if(increasing) {
    brightness++;
    if(brightness >= 255) increasing = false;
  } else {
    brightness--;
    if(brightness <= 1) increasing = true;
  }
  
  leds[ledIndex] = color;
  leds[ledIndex].fadeLightBy(255 - brightness);
}

// ===========================================================================
void neopixels_all_clear(CRGB color) {
// ===========================================================================
  fill_solid(leds, CONFIG_NEOPIXELS_NB_LEDS, color);
  FastLED.show();
}

// ===========================================================================
void neopixels_all_blink(CRGB color, uint8_t blinks, uint16_t delayTime) {
// ===========================================================================
  for(uint8_t i=0; i<blinks; i++) {
    neopixels_all_clear(color);
    FastLED.delay(delayTime);
    neopixels_all_clear(CRGB::Black);
    FastLED.delay(delayTime);
  }
}

// ===========================================================================
void neopixels_demo() {
// ===========================================================================
  // LED 0: Rainbow cycle
  neopixels_rainbow(0);
  
  // LED 1: Pulsing red
  neopixels_pulse(1, CRGB::Red);
  
  FastLED.show();
  FastLED.delay(20);  // Control speed
}
