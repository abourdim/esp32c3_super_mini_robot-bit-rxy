#include "01_includes.h"

// you can enable debug logging to Serial at 115200
// #define REMOTEXY__DEBUGLOG    
#define USE_BLE
#ifdef USE_BLE
  // RemoteXY select connection mode and include library 
  #define REMOTEXY_MODE__ESP32CORE_BLE
  #include <BLEDevice.h>

  // RemoteXY connection settings 
  #define REMOTEXY_BLUETOOTH_NAME CONFIG_REMOTEXY_BLUETOOTH_NAME

#else
  #define REMOTEXY_MODE__WIFI_POINT
  #include <WiFi.h>
  // RemoteXY connection settings 
  #define REMOTEXY_WIFI_SSID "wdy_wifi_01"
  #define REMOTEXY_WIFI_PASSWORD "12345678"
  #define REMOTEXY_SERVER_PORT 6377
#endif

#include <RemoteXY.h>

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t RemoteXY_CONF[] =   // 329 bytes
  { 255,13,0,7,0,66,1,19,0,0,0,0,180,2,130,200,108,200,1,1,
  14,0,1,1,1,128,20,40,253,24,24,9,8,31,119,111,114,107,115,104,
  111,112,45,100,105,121,46,111,114,103,58,32,68,69,77,79,95,86,49,46,
  48,0,69,57,173,24,24,2,175,20,20,0,138,4,101,140,19,58,18,99,
  15,82,0,1,26,72,90,113,36,36,81,107,24,24,108,166,140,179,26,0,
  0,0,0,0,0,52,67,0,0,0,0,135,0,0,0,0,0,0,140,66,
  94,0,0,140,66,0,0,180,66,36,0,0,180,66,0,0,52,67,1,51,
  24,31,31,5,39,24,24,0,24,31,98,117,122,122,101,114,0,10,96,27,
  24,24,43,38,24,24,112,135,26,31,108,101,100,49,0,31,4,14,141,19,
  57,89,140,12,58,0,1,26,72,7,114,36,36,83,104,24,24,108,166,140,
  179,26,0,0,0,0,0,0,52,67,0,0,0,0,135,0,0,0,0,0,
  0,140,66,94,0,0,140,66,0,0,180,66,36,0,0,180,66,0,0,52,
  67,70,53,116,27,23,46,125,18,18,41,136,1,136,32,67,44,151,44,17,
  42,73,33,10,86,31,136,6,84,61,45,45,255,61,40,40,2,26,10,48,
  75,34,18,44,74,24,24,49,136,36,31,79,78,0,31,79,70,70,0,10,
  13,27,24,24,79,29,24,24,112,1,26,31,108,101,100,50,0,31,6,1,
  61,45,45,68,63,45,45,2,26 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  uint8_t button_01; // =1 if button pressed, else =0
  int8_t slider_m1; // from 0 to 100
  uint8_t button_buzzer; // =1 if button pressed, else =0
  uint8_t pushSwitch_led1; // =1 if state is ON, else =0
  int8_t slider_m2; // from 0 to 100
  uint8_t rgb_r_1; // =0..255 Red color value
  uint8_t rgb_g_1; // =0..255 Green color value
  uint8_t rgb_b_1; // =0..255 Blue color value
  uint8_t pushSwitch_neopixels; // =1 if state is ON, else =0
  uint8_t pushSwitch_led2; // =1 if state is ON, else =0
  uint8_t rgb_r_2; // =0..255 Red color value
  uint8_t rgb_g_2; // =0..255 Green color value
  uint8_t rgb_b_2; // =0..255 Blue color value

    // output variables
  int16_t sound_01; // =0 no sound, else ID of sound, =1001 for example, look sound list in app
  int8_t circularBar_speed_m1; // from 0 to 180
  int8_t circularBar_speed_m2; // from 0 to 180
  uint8_t led_button_status; // from 0 to 2
  int16_t value_ultrasonic; // -32768 .. +32767

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;   

#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

// ===========================================================================
void remotexy_init(void) {
// ===========================================================================
  // Initialize RemoteXY
  RemoteXY_Init();
}

// ===========================================================================
void remotexy_handler(void) {
// ===========================================================================
  // Initialize RemoteXY
  RemoteXY_Handler();
}


// ===========================================================================
uint8_t remotexy_get_pushSwitch_neopixels(){
// ===========================================================================
	return RemoteXY.pushSwitch_neopixels;
}

// ===========================================================================
void remotexy_set_pushSwitch_neopixels(uint8_t p_pushSwitch_neopixels){
// ===========================================================================
	RemoteXY.pushSwitch_neopixels = p_pushSwitch_neopixels;
}


// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
uint8_t remotexy_get_button_01(){
// ===========================================================================
	return RemoteXY.button_01;
}

// ===========================================================================
int8_t remotexy_get_slider_m1(){
	return RemoteXY.slider_m1;
}

// ===========================================================================
int8_t remotexy_get_slider_m2(){
	return RemoteXY.slider_m2;
}

#if 0
// ===========================================================================
int16_t remotexy_get_edit_m1(){
	return RemoteXY.edit_m1;
}

// ===========================================================================
int16_t remotexy_get_edit_m2(){
	return RemoteXY.edit_m2;
}
#endif

// ===========================================================================
uint8_t remotexy_get_button_buzzer(){
	return RemoteXY.button_buzzer;
}

// ===========================================================================
uint8_t remotexy_get_pushSwitch_led_1(){
	return RemoteXY.pushSwitch_led1;
}

// ===========================================================================
uint8_t remotexy_get_pushSwitch_led2(){
	return RemoteXY.pushSwitch_led2;
}

// ===========================================================================
uint8_t remotexy_get_rgb_b_1(){
	return RemoteXY.rgb_b_1;
}

// ===========================================================================
uint8_t remotexy_get_rgb_g_1(){
	return RemoteXY.rgb_g_1;
}

// ===========================================================================
uint8_t remotexy_get_rgb_r_1(){
	return RemoteXY.rgb_r_1;
}

// ===========================================================================
uint8_t remotexy_get_rgb_b_2(){
	return RemoteXY.rgb_b_2;
}

// ===========================================================================
uint8_t remotexy_get_rgb_g_2(){
	return RemoteXY.rgb_g_2;
}

// ===========================================================================
uint8_t remotexy_get_rgb_r_2(){
	return RemoteXY.rgb_r_2;
}

// ===========================================================================
int16_t remotexy_get_sound_01(){
	return RemoteXY.sound_01;
}

// ===========================================================================
int8_t remotexy_get_circularBar_speed_m1(){
	return RemoteXY.circularBar_speed_m1;
}

// ===========================================================================
int8_t remotexy_get_circularBar_speed_m2(){
	return RemoteXY.circularBar_speed_m2;
}

// ===========================================================================
uint8_t remotexy_get_led_button_status(){
	return RemoteXY.led_button_status;
}

// ===========================================================================
int16_t remotexy_get_value_ultrasonic(){
	return RemoteXY.value_ultrasonic;
}

// ===========================================================================
uint8_t remotexy_get_connect_flag(){
	return RemoteXY.connect_flag;
}

// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
void remotexy_set_button_01(uint8_t p_button_01){
// ===========================================================================
	RemoteXY.button_01 = p_button_01;
}

// ===========================================================================
void remotexy_set_slider_m1(int8_t p_slider_m1){
	RemoteXY.slider_m1 = p_slider_m1;
}

// ===========================================================================
void remotexy_set_slider_m2(int8_t p_slider_m2){
	RemoteXY.slider_m2 = p_slider_m2;
}

#if 0
// ===========================================================================
void remotexy_set_edit_m1(int16_t p_edit_m1){
	RemoteXY.edit_m1 = p_edit_m1;
}

// ===========================================================================
void remotexy_set_edit_m2(int16_t p_edit_m2){
	RemoteXY.edit_m2 = p_edit_m2;
}
#endif

// ===========================================================================
void remotexy_set_button_buzzer(uint8_t p_button_buzzer){
	RemoteXY.button_buzzer = p_button_buzzer;
}

// ===========================================================================
void remotexy_set_pushSwitch_led_1(uint8_t p_pushSwitch_led_1){
	RemoteXY.pushSwitch_led1 = p_pushSwitch_led_1;
}

// ===========================================================================
void remotexy_set_pushSwitch_led2(uint8_t p_pushSwitch_led2){
	RemoteXY.pushSwitch_led2 = p_pushSwitch_led2;
}

// ===========================================================================
void remotexy_set_rgb_b_1(uint8_t p_rgb_b_1){
	RemoteXY.rgb_b_1 = p_rgb_b_1;
}

// ===========================================================================
void remotexy_set_rgb_g_1(uint8_t p_rgb_g_1){
	RemoteXY.rgb_g_1 = p_rgb_g_1;
}

// ===========================================================================
void remotexy_set_rgb_r_1(uint8_t p_rgb_r_1){
	RemoteXY.rgb_r_1 = p_rgb_r_1;
}

// ===========================================================================
void remotexy_set_rgb_b_2(uint8_t p_rgb_b_2){
	RemoteXY.rgb_b_2 = p_rgb_b_2;
}

// ===========================================================================
void remotexy_set_rgb_g_2(uint8_t p_rgb_g_2){
	RemoteXY.rgb_g_2 = p_rgb_g_2;
}

// ===========================================================================
void remotexy_set_rgb_r_2(uint8_t p_rgb_r_2){
	RemoteXY.rgb_r_2 = p_rgb_r_2;
}

// ===========================================================================
void remotexy_set_sound_01(int16_t p_sound_01){
	RemoteXY.sound_01 = p_sound_01;
}

// ===========================================================================
void remotexy_set_circularBar_speed_m1(int8_t p_circularBar_speed_m1){
	RemoteXY.circularBar_speed_m1 = p_circularBar_speed_m1;
}

// ===========================================================================
void remotexy_set_circularBar_speed_m2(int8_t p_circularBar_speed_m2){
	RemoteXY.circularBar_speed_m2 = p_circularBar_speed_m2;
}

// ===========================================================================
void remotexy_set_led_button_status(uint8_t p_led_button_status){
	RemoteXY.led_button_status = p_led_button_status;
}

// ===========================================================================
void remotexy_set_value_ultrasonic(int16_t p_value_ultrasonic){
	RemoteXY.value_ultrasonic = p_value_ultrasonic;
}

// ===========================================================================
void remotexy_set_connect_flag(uint8_t p_connect_flag){
	RemoteXY.connect_flag = p_connect_flag;
}
