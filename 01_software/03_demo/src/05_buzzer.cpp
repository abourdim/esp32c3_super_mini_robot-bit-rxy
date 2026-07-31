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

  digitalWrite(CONFIG_PIN_BUZZER, LOW);   
}

// --------------------------
// PREDEFINED SOUND PATTERNS
// --------------------------

// ===========================================================================
void buzzer_soundOK() {
// ===========================================================================
  
  playTone(20, 10, 0);
  
  // Short ascending tone
  // playTone(800, 100, 0);
  // playTone(1200, 100, 0);
  // playTone(1600, 100, 0);
}

// ===========================================================================
void buzzer_soundError() {
// ===========================================================================
  // Long descending tone
  playTone(1200, 300, 0);
  playTone(800, 300, 0);
  playTone(400, 300, 0);
}

// ===========================================================================
void buzzer_soundWarning() {
// ===========================================================================
  // Pulsing medium tone
  for (int32_t i = 0; i < 3; i++) {
    playTone(1000, 200, 0);
    delay(100);
  }
}

// ===========================================================================
void buzzer_soundStartup() {
// ===========================================================================
  // Happy ascending triple beep
  playTone(523, 150, 50);  // C5
  playTone(659, 150, 50);  // E5
  playTone(784, 300,0);      // G5
}

// ===========================================================================
void buzzer_soundButtonPress() {
// ===========================================================================
  // Short click sound
  //playTone(1500, 30, 0);
  playTone(10, 100, 0);
}

// ===========================================================================
void buzzer_soundAlert() {
// ===========================================================================
  // Fast repeating pulses
  for (int32_t i = 0; i < 8; i++) {
    playTone(1200, 50, 0);
    delay(50);
  }
}

// --------------------------
// DEMONSTRATION LOOP
// --------------------------

// ===========================================================================
void buzzer_soundConnected() {
// ===========================================================================
  playTone(CONFIG_PIN_BUZZER, 2000, 100);  // 2kHz tone for 100ms  
}

// ===========================================================================
void buzzer_soundDisConnected() {
// ===========================================================================
  playTone(CONFIG_PIN_BUZZER, 1500, 300);  // Lower pitch for disconnect
}

// ===========================================================================
void buzzer_demo() {
// ===========================================================================
  buzzer_soundStartup();
  delay(1000);
  
  buzzer_soundOK();
  delay(1000);
  
  buzzer_soundWarning();
  delay(1000);
  
  buzzer_soundError();
  delay(1000);
  
  buzzer_soundAlert();
  delay(2000);
}