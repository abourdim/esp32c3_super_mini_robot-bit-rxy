#include "01_includes.h"

// ===========================================================================
void buzzer_init() {
// ===========================================================================
  pinMode(CONFIG_PIN_BUZZER, OUTPUT);
  digitalWrite(CONFIG_PIN_BUZZER, LOW);
}

// ===========================================================================
void playTone(int32_t freq, int32_t duration, int32_t pause) {
// ===========================================================================
  
  // ?????????????????????????????
  // return;
  // ?????????????????????????????

  if (freq <= 0) {
    delay(duration);
    return;
  }
  
  pinMode(CONFIG_PIN_BUZZER, OUTPUT);
  
  int32_t period = 1000000 / freq;
  int32_t pulseWidth = period / 2;
  int32_t elapsedTime = 0;
  int32_t durationMicros = duration * 1000L; 
  
  while (elapsedTime < durationMicros) {
    digitalWrite(CONFIG_PIN_BUZZER, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(CONFIG_PIN_BUZZER, LOW);
    delayMicroseconds(pulseWidth);
    elapsedTime += period;
  }
  
  if (pause > 0) {
    delay(pause);
  }  
}

// --------------------------
// PREDEFINED SOUND PATTERNS
// --------------------------

// ===========================================================================
void buzzer_beep() {
// ===========================================================================
  // Happy ascending triple beep
  playTone(CONFIG_BUZZER_FREQ, 20, 0);  // C5  
}

// ===========================================================================
void buzzer_demo() {
// ===========================================================================
  buzzer_beep(); 
}