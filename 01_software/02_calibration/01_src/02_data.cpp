///////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

#include "01_includes.h"

// Add these with other global variables
uint32_t lastDisconnectedFlash = 0;
bool disconnectedFlashState = false;

int32_t rawADCValue = 0;
float batteryVoltage = 0.0;
int32_t batteryPercentage = 0;

bool event_connection_state_flag_bo = false;  // To track connection state changes

// Servo objects
Servo servoLeft;
Servo servoRight;

int32_t duration;
float distance_cm;

uint32_t previousMillis = 0;
const int32_t interval = 500;  // 500ms

uint32_t startMillis = 0;
uint32_t elapsedSeconds = 0;
uint32_t elapsedMillis = 0;
int32_t flag_show_ble_name=0;

// ===========================================================================

uint32_t g_previous_millis_u32 = 0;
const int32_t g_interval_i32 = 500;  // 500ms

uint32_t g_ultrasonic_distance_cm;

int32_t g_battery_raw_adc;
float g_battery_voltage;
int32_t g_battery_percentage;

uint32_t g_elapsed_time_startup_millis;
uint32_t g_elapsed_time_hours;
uint32_t g_elapsed_time_minutes;
uint32_t g_elapsed_time_seconds;

uint32_t g_speed_s1;
uint32_t g_speed_s2;

