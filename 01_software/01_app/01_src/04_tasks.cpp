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
void tasks_buttons(void) {
// ===========================================================================
  if( remotexy_get_button_01()) {
    g_remotexy_sound1_val = REMOTEXY_SOUND_ALARM;
    neopixels_all_blink(CRGB::Green, 1, 20);    
    buzzer_beep();

    //remotexy_set_joystick_01_x( map(CONFIG_SERVO_SPEED_STOP_LEFT, 0, 180, -100, 100));
    //remotexy_set_joystick_01_y(map(CONFIG_SERVO_SPEED_STOP_RIGHT, 0, 180, -100, 100 ));
    
  }  
  if( button_pressed() ) {
    g_remotexy_sound1_val = REMOTEXY_SOUND_BEEP_SHORT;
    neopixels_all_blink(CRGB::Red, 1, 20);
    buzzer_beep();  
        
  }  
}

// ===========================================================================
void tasks_rmotexy_sound(void) {
// ===========================================================================
  static uint32_t s_previous_millis_u32 =0;
  static uint8_t s_state_u8 =0;

  uint32_t l_current = millis(); 

  if ( g_remotexy_sound1_val ) {
    remotexy_set_sound_01(g_remotexy_sound1_val);
    g_remotexy_sound1_val = 0;
    s_previous_millis_u32 = l_current; 
    return;
  }

  if (l_current - s_previous_millis_u32 >= 2000) {
    s_previous_millis_u32 = l_current; 
    remotexy_set_sound_01(g_remotexy_sound1_val);
  }     
}

// ===========================================================================
void tasks_joysticks(void) {
// ===========================================================================

// Map joystick values to servo speeds
  int32_t x;
  int32_t y;

  

  uint32_t x_abs;
  uint32_t y_abs;
  uint32_t max_x_y;

  // Calculate motor speeds based on joystick position

  x = remotexy_get_joystick_01_x() ;
  y = remotexy_get_joystick_01_y();

  if(x>=0){ x_abs=x; }else { x_abs=-x; }
  if(y>=0){ y_abs=y; }else { y_abs=-y; } 
  if(x_abs>y_abs) { max_x_y = x_abs; } else { max_x_y = y_abs; }  

  // NOTE: do not call remotexy_set_*() here — tasks_joysticks() runs on
  // every loop() iteration. Calling a BLE sendValue() that often (even
  // gated by s_sendingCfg) was found to flood NimBLE's notify() with
  // rc=6 (BLE_HS_ENOMEM) continuously during a connected session. Just
  // stash the value; tasks_remotexy() sends it on the throttled interval.
  g_joystick_speed_pct = max_x_y;


   // Read joystick values into joystick_01_x and joystick_01_y here
  
  int leftSpeed = y+x;  // forward/backward + turn
  int rightSpeed = y-x; // forward/backward - turn

  // Clamp speeds to -100 to 100
  leftSpeed = constrain(leftSpeed, -100, 100);
  rightSpeed = constrain(rightSpeed, -100, 100);

  // Speed slider (DRIVE zone) scales the mix rather than clipping it, so
  // steering geometry is preserved at every ceiling — a hard clamp would
  // straighten out turns as the cap came down.
  const uint8_t cap = remotexy_get_speed_cap();
  if (cap < 100) {
    leftSpeed  = (leftSpeed  * cap) / 100;
    rightSpeed = (rightSpeed * cap) / 100;
  }

  if (! remotexy_get_connect_flag()) {  // Only if connected
    rightSpeed = 0;
    leftSpeed = 0;
  }

  // Map -100 to 100 into servo signals: 0 to 180
  // Assuming 90 = stop, <90 = reverse, >90 = forward
  int leftPulse = map(leftSpeed , -100, 100, 0, 180);
  // Right wheel is mounted as a mirror image of the left one on a
  // differential-drive chassis, so the same rightSpeed sign needs an
  // inverted pulse range to actually spin the same physical direction.
  int rightPulse = map(rightSpeed, -100, 100, 180, 0);

  moveServos(leftPulse+CONFIG_SERVO_SPEED_STOP_LEFT_OFFSET, rightPulse+CONFIG_SERVO_SPEED_STOP_RIGHT_OFFSET);
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

// ===========================================================================
void tasks_remotexy(void) {
// ===========================================================================
  remotexy_set_onlineGraph_01_distance(g_ultrasonic_distance_cm);
  remotexy_set_onlineGraph_02_speed(g_joystick_speed_pct);
  remotexy_set_onlineGraph_03_battery(g_battery_percentage);

  // Widgets added with the zoned layout. Each one self-gates on the Telemetry
  // level, so this list stays flat rather than nesting the whole block.
  remotexy_send_graph_distance(g_ultrasonic_distance_cm);
  remotexy_send_obstacle_alert(g_ultrasonic_distance_cm);
  remotexy_send_system_labels();
  remotexy_send_button_state();
  remotexy_send_link_rssi();
  remotexy_send_control_echo();
  remotexy_send_oled_mirror();
  remotexy_send_led_state();
  // Must stay last: ends the forced full refresh that follows a CFG transfer.
  remotexy_telemetry_end();
}




 

