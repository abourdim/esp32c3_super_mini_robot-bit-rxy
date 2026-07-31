#include "01_includes.h"

CRGB leds[CONFIG_NEOPIXELS_NB_LEDS];     // LED array

void neopixels_init() {
  FastLED.addLeds<LED_TYPE, CONFIG_PIN_NEOPIXEL, COLOR_ORDER>(leds, CONFIG_NEOPIXELS_NB_LEDS);
  FastLED.setBrightness(100);  // Set brightness (0-255)
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("FastLED NeoPixel example started");
  #endif
}

void neopixels_fill( uint8_t p_r_1, uint8_t p_g_1, uint8_t p_b_1,
                         uint8_t p_r_2, uint8_t p_g_2, uint8_t p_b_2 ) {
                          int i;
  for(i=0; i<32; i++) {
      leds[i]= CRGB( p_r_1, p_g_1, p_b_1 );
      leds[i+32]= CRGB( p_r_2, p_g_2, p_b_2 );
  }
}

void neopixels_moving_dot() {
  static int pos = 0;
  fill_solid(leds, CONFIG_NEOPIXELS_NB_LEDS, CRGB::Black);
  leds[pos] = CRGB::Blue;
  FastLED.show();
  pos = (pos + 1) % CONFIG_NEOPIXELS_NB_LEDS;
  delay(50);
}

void neopixels_fading_sparkles() {
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 64);   // fade previous pixels
  int pos = random(CONFIG_NEOPIXELS_NB_LEDS);
  leds[pos] = CHSV(random(160, 180), 255, 255);  // soft blue sparkle
  FastLED.show();
  delay(100);
}

void neopixels_fading_sparkles2(uint8_t p_r_1, uint8_t p_g_1, uint8_t p_b_1) {
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 64);   // fade previous pixels
  int pos = random(CONFIG_NEOPIXELS_NB_LEDS);
  leds[pos] = CHSV(p_r_1, p_g_1, p_b_1);  // soft blue sparkle
  FastLED.show();
  delay(100);
}

void neopixels_soft_rainbow() {
  static uint8_t hue = 0;
  fill_rainbow(leds, CONFIG_NEOPIXELS_NB_LEDS, hue, 7);
  FastLED.show();
  hue += 2;
  delay(30);
}

void neopixels_custom_waves() {
  static uint8_t pos = 0;
  fill_solid(leds, CONFIG_NEOPIXELS_NB_LEDS, CRGB::Black);
  for (int i = 0; i < 64; i++) {
    uint8_t brightness = 32 * sin8((i * 8 + pos) & 0xFF) / 255;
    leds[i] = CHSV(160, 255, brightness);
  }
  FastLED.show();
  pos += 2;
  delay(30);
}

// neopixels_rainbow effect for single LED
void neopixels_rainbow(uint8_t ledIndex) {
  static uint8_t hue = 0;
  leds[ledIndex] = CHSV(hue++, 255, 255);
  if(hue >= 255) hue = 0;
}

// Pulsing effect for single LED
void neopixels_pulse(uint8_t ledIndex, CRGB color) {
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

// Set both LEDs to solid color
void neopixels_setBoth(CRGB color) {
  fill_solid(leds, CONFIG_NEOPIXELS_NB_LEDS, color);
  FastLED.show();
}

// Simple blink pattern
void neopixels_blinkBoth(CRGB color, uint8_t blinks, uint16_t delayTime) {
  for(uint8_t i=0; i<blinks; i++) {
    neopixels_setBoth(color);
    FastLED.delay(delayTime);
    neopixels_setBoth(CRGB::Black);
    FastLED.delay(delayTime);
  }
}

void neopixels_demo() {
  // LED 0: neopixels_rainbow cycle
  neopixels_rainbow(0);
  
  // LED 1: Pulsing red
  neopixels_pulse(1, CRGB::Red);
  
  FastLED.show();
  FastLED.delay(20);  // Control speed
}
