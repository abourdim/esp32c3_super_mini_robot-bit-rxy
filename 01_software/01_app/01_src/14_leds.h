#ifndef __DEF_INCLUDE_LEDS_H__
#define __DEF_INCLUDE_LEDS_H__

extern void leds_init(void);
extern void leds_update(void);

// Logical state of the two board LEDs, for the panel indicators. Reflects
// which LED is active rather than the blink phase -- see 14_leds.cpp.
extern uint8_t leds_state_r(void);
extern uint8_t leds_state_g(void);

#endif // __DEF_INCLUDE_LEDS_H__