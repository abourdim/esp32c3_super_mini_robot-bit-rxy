#ifndef __DEF_INCLUDE_BUZZER_H__
#define __DEF_INCLUDE_BUZZER_H__

// Function declarations
void buzzer_init();
void playTone(int32_t freq, int32_t duration, int32_t pause);  // Remove default parameter here

extern void buzzer_beep(void);

// Sound patterns
extern void buzzer_soundOK();
extern void buzzer_soundError();
extern void buzzer_soundWarning();
extern void buzzer_soundStartup();
extern void buzzer_soundButtonPress();
extern void buzzer_soundAlert();

void buzzer_soundConnected();
void buzzer_soundDisConnected();

// Demo function
void buzzer_demo();

#endif // __DEF_INCLUDE_BUZZER_H__