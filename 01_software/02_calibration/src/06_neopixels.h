#ifndef __DEF_INCLUDE_NEOPIXELS_H__
#define __DEF_INCLUDE_NEOPIXELS_H__

// CRGB::Red     // Full red
// CRGB::Green   // Full green (not GREEN)
// CRGB::Blue    // Full blue
// CRGB::White   // White
// CRGB::Black   // Off
// CRGB::Yellow  // Yellow
// CRGB::Purple  // Purple
// CRGB::Cyan    // Cyan

#define LED_TYPE WS2812  // LED chip type
#define COLOR_ORDER GRB  // Color order (common for WS2812)

void neopixels_init();
void neopixels_rainbow(uint8_t ledIndex);
void neopixels_pulse(uint8_t ledIndex, CRGB color);
void neopixels_all_clear(CRGB color);
void neopixels_all_blink(CRGB color, uint8_t blinks, uint16_t delayTime);
void neopixels_demo();

#endif // __DEF_INCLUDE_NEOPIXELS_H__