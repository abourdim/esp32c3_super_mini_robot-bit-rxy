#include "01_includes.h"

// ===========================================================================
void ultrasonic_init(void) {
// ===========================================================================
  // Setup ultrasonic pins
  pinMode(CONFIG_PIN_SR04_TRIG, OUTPUT);
  pinMode(CONFIG_PIN_SR04_ECHO, INPUT);
}

// ===========================================================================
void ultrasonic_get_distance(void) {
// ===========================================================================
  int32_t l_duration;

  digitalWrite(CONFIG_PIN_SR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(CONFIG_PIN_SR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(CONFIG_PIN_SR04_TRIG, LOW);

  //duration = pulseIn(CONFIG_PIN_SR04_ECHO, HIGH, 30000);  // timeout after 30,000 µs = 30 ms (about 5 meters)
  l_duration = pulseIn(CONFIG_PIN_SR04_ECHO, HIGH, 15000);
  g_ultrasonic_distance_cm = l_duration * 0.034 / 2.0;  
}
