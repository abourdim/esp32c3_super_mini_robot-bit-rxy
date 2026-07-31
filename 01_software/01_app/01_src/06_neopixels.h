#ifndef __DEF_INCLUDE_NEOPIXELS_H__
#define __DEF_INCLUDE_NEOPIXELS_H__

extern CRGB leds[CONFIG_NEOPIXELS_MAX_LEDS];    // LED array (max size)

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

extern void neopixels_KnightRiderRedEyewithTail();
extern void neopixels_KnightRiderRedEyewithTail2();
extern void neopixels_duel_eye();

void neopixels_animate() ;

void neopixels_demo_legacy();

#endif // __DEF_INCLUDE_NEOPIXELS_H__