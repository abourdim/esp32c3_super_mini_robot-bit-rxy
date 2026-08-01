#ifndef __DEF_INCLUDE_BUZZER_H__
#define __DEF_INCLUDE_BUZZER_H__

// Function declarations
void buzzer_init();
void playTone(int32_t freq, int32_t duration, int32_t pause);  // Remove default parameter here
extern void buzzer_beep(void);

// Demo function
void buzzer_demo();

#endif // __DEF_INCLUDE_BUZZER_H__