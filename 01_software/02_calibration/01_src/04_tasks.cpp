#include "01_includes.h"

// ===========================================================================
void tasks_buzzer(void) {
// ===========================================================================

}

// ===========================================================================
void tasks_connect(void) {
// ===========================================================================
  events_connection_state_enum l_currentState;

    l_currentState = events_get_connect_flag();

    if(l_currentState == EVENTS_CONNECTION_NO_CHANGE){
      return;
    }

    if(l_currentState == EVENTS_CONNECTION_CONNECTED) {
      buzzer_beep();
      return;
    }

    if(l_currentState == EVENTS_CONNECTION_DISCONNECTED) {

      buzzer_beep();  // Single beep
    }
}

// ===========================================================================
void tasks_calibration(void) {
// ===========================================================================
  // Sliders/edit-fields write s_pulse_left/right directly (0-180, no arcade
  // mixing) -- see 03_bit-rxy.cpp. Drive the servos with that raw value so
  // you can find each wheel's true stop point by eye.
  int16_t leftPulse  = remotexy_get_pulse_left();
  int16_t rightPulse = remotexy_get_pulse_right();

  if (!remotexy_get_connect_flag()) {  // stop if nobody's connected
    leftPulse  = 90;
    rightPulse = 90;
  }

  // Don't pre-assign g_speed_s1/s2 here -- moveServos() itself compares its
  // arguments against those same globals to decide whether the value
  // actually changed, and only then writes the servo + updates them. Setting
  // them first would make moveServos() always see "unchanged" and skip the
  // actual PWM write.
  moveServos(leftPulse, rightPulse);
}

// ===========================================================================
void tasks_battery(void) {
// ===========================================================================
  g_battery_raw_adc = analogRead(CONFIG_PIN_BATTERY_LEVEL);
  g_battery_voltage = readBatteryVoltage();
  g_battery_percentage = calculateBatteryPercentage(g_battery_voltage);
}

// ===========================================================================
void tasks_elapsed_time(void) {
// ===========================================================================
  uint32_t l_millis;
  uint32_t l_elapsed;
  uint32_t l_total_seconds;

  l_millis = millis();

  l_elapsed = l_millis - g_elapsed_time_startup_millis;

  l_total_seconds = l_elapsed / 1000;
  g_elapsed_time_hours   = l_total_seconds / 3600;
  g_elapsed_time_minutes = (l_total_seconds % 3600) / 60;
  g_elapsed_time_seconds = l_total_seconds % 60;

}
