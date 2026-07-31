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

extern void neopixels_init();

extern void neopixels_fill( uint8_t p_r_1, uint8_t p_g_1, uint8_t p_b_1,
                                uint8_t p_r_2, uint8_t p_g_2, uint8_t p_b_2 );

extern void neopixels_rainbow(uint8_t ledIndex);
extern void neopixels_pulse(uint8_t ledIndex, CRGB color);
extern void neopixels_setBoth(CRGB color);
extern void neopixels_blinkBoth(CRGB color, uint8_t blinks, uint16_t delayTime);
extern void neopixels_demo();


extern void neopixels_moving_dot();
extern void neopixels_fading_sparkles();
extern void neopixels_soft_rainbow();
extern void neopixels_custom_waves();

extern void neopixels_fading_sparkles2(uint8_t p_r_1, uint8_t p_g_1, uint8_t p_b_1);

#endif // __DEF_INCLUDE_NEOPIXELS_H__