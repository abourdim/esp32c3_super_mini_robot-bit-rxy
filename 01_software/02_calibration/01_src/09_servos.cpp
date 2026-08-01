#include "01_includes.h"


// ===========================================================================
void moveServos(int32_t leftSpeed, int32_t rightSpeed) {
// ===========================================================================
  leftSpeed = constrain(leftSpeed, 0, 180);
  rightSpeed = constrain(rightSpeed, 0, 180);

  // moveServos() runs from tasks_calibration() on every loop() iteration.
  // Skip redundant PWM writes when the value hasn't changed, and cap the
  // rate to ~50Hz otherwise — servos don't respond faster than that, and
  // the app itself already throttles incoming joystick messages to ~200ms.
  static uint32_t s_lastWriteMillis = 0;
  static bool     s_hasWritten = false;
  uint32_t now = millis();
  bool changed = (leftSpeed != (int32_t)g_speed_s1) || (rightSpeed != (int32_t)g_speed_s2);
  if (changed && (!s_hasWritten || (now - s_lastWriteMillis >= 20))) {
    servoLeft.write(leftSpeed);
    servoRight.write(rightSpeed);
    g_speed_s1 = leftSpeed;
    g_speed_s2 = rightSpeed;
    s_lastWriteMillis = now;
    s_hasWritten = true;
  }

  // For debugging
  #ifdef DEF_DERIAL_DEBUG
  // Serial.printf("Left: %d, Right: %d\n", leftSpeed, rightSpeed);
  #endif
}

// ===========================================================================
void stopServos() {
// ===========================================================================
  moveServos(CONFIG_SERVO_SPEED_STOP_LEFT, CONFIG_SERVO_SPEED_STOP_RIGHT);
}
