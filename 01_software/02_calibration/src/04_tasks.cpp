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
      buzzer_soundOK();
      return;
    } 
    
    if(l_currentState == EVENTS_CONNECTION_DISCONNECTED) {
     
      buzzer_soundWarning();  // Single beep      
    }  
}

// ===========================================================================
void tasks_buttons(void) {
// ===========================================================================
  
  if(remotexy_get_button_01()) {

    // remotexy_set_sound_01(REMOTEXY_SOUND_ALARM);
    buzzer_soundAlert();
    neopixels_all_blink(CRGB::Green, 3, 20);
    flag_show_ble_name = 10;
    
  }else if( button_pressed() ) {
    // remotexy_set_sound_01(REMOTEXY_SOUND_BEEP_SHORT);
    buzzer_soundButtonPress();
    neopixels_all_blink(CRGB::Blue, 3, 20);
    flag_show_ble_name = 10;
    
  } else {
    // remotexy_set_sound_01(0); 
  }
}

#if 0
// ===========================================================================
void tasks_joysticks(void) {
// ===========================================================================

// Map joystick values to servo speeds
  int32_t x;
  int32_t y;

  int32_t rightSpeed;
  int32_t leftSpeed;

  int32_t x_abs;
  int32_t y_abs;
  int32_t max_x_y;

  // Calculate motor speeds based on joystick position

  x = remotexy_get_joystick_01_x();
  y = remotexy_get_joystick_01_y();

  if(x>=0){ x_abs=x; }else { x_abs=-x; }
  if(y>=0){ y_abs=y; }else { y_abs=-y; } 
  if(x_abs>y_abs) { max_x_y = x_abs; } else { max_x_y = y_abs; }  

  remotexy_set_circularBar_01(max_x_y);
  remotexy_set_onlineGraph_02_speed(max_x_y);
  
  if (abs(x) > 10 || abs(y) > 10) {  // Deadzone threshold
      // Differential drive calculation
      rightSpeed = y + x;
      leftSpeed = y - x;
  } else {
     rightSpeed = CONFIG_SERVO_SPEED_STOP_RIGHT;
     leftSpeed = CONFIG_SERVO_SPEED_STOP_LEFT;
  }
  
  if (! remotexy_get_connect_flag()) {  // Only if connected
    leftSpeed = CONFIG_SERVO_SPEED_STOP_LEFT;
    rightSpeed = CONFIG_SERVO_SPEED_STOP_RIGHT;
  }
  moveServos(leftSpeed, rightSpeed);
}
#endif

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

#if 0
// ===========================================================================
void tasks_remotexy(void) {
// ===========================================================================
  remotexy_set_onlineGraph_01_distance(g_ultrasonic_distance_cm);
  remotexy_set_onlineGraph_03_battery(g_battery_voltage*10);
}
#endif
