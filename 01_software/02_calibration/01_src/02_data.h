#ifndef __DEF_INCLUDE_DATA_H__
#define __DEF_INCLUDE_DATA_H__

// Add these with other global variables
extern uint32_t lastDisconnectedFlash;
extern bool disconnectedFlashState;

extern int32_t rawADCValue;
extern float batteryVoltage;
extern int32_t batteryPercentage;


extern bool event_connection_state_flag_bo;

// Servo objects
extern Servo servoLeft;
extern Servo servoRight;

extern int32_t duration;
extern float distance_cm;

extern uint32_t previousMillis;
extern const int32_t interval;

extern uint32_t startMillis;
extern uint32_t elapsedSeconds;
extern uint32_t elapsedMillis;


extern int32_t flag_show_ble_name;

extern uint32_t g_speed_s1;
extern uint32_t g_speed_s2;

// ===========================================================================

extern uint32_t g_previous_millis_u32;
extern const int32_t g_interval_i32;  // 500ms

extern uint32_t g_ultrasonic_distance_cm;

extern int32_t g_battery_raw_adc;
extern float g_battery_voltage;
extern int32_t g_battery_percentage;

extern uint32_t g_elapsed_time_startup_millis;
extern uint32_t g_elapsed_time_hours;
extern uint32_t g_elapsed_time_minutes;
extern uint32_t g_elapsed_time_seconds;

#endif // __DEF_INCLUDE_DATA_H__