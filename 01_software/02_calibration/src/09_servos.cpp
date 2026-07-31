#include "01_includes.h"


// ===========================================================================
void moveServos(int32_t leftSpeed, int32_t rightSpeed) {
// ===========================================================================
  leftSpeed = constrain(leftSpeed, 0, 180);
  rightSpeed = constrain(rightSpeed, 0, 180);
  
  servoLeft.write(leftSpeed);
  servoRight.write(rightSpeed);
  
  // For debugging
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("Left: %d, Right: %d\n", leftSpeed, rightSpeed);
  #endif
}

// ===========================================================================
void stopServos() {
// ===========================================================================
  moveServos(90, 90);
}
