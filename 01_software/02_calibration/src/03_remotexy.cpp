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
uint8_t RemoteXY_CONF[] =   // 119 bytes
  { 255,7,0,0,0,112,0,19,0,0,0,0,178,2,130,200,108,200,1,1,
  5,0,1,1,1,128,20,40,253,24,24,9,8,31,87,111,114,107,115,104,
  111,112,45,100,105,121,32,32,45,32,67,97,108,105,98,114,97,116,105,111,
  110,32,45,32,118,49,46,48,0,4,78,58,39,140,18,99,15,82,0,136,
  26,7,68,29,60,27,62,80,33,10,85,64,24,136,7,2,29,63,27,81,
  130,27,12,85,64,24,1,4,15,58,40,140,67,40,33,160,0,1,26 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  uint8_t button_01; // =1 if button pressed, else =0
  int8_t slider_01_speed_s1; // from 0 to 100
  int16_t edit_01_s1; // -32768 .. +32767
  int16_t edit_02_s2; // -32768 .. +32767
  int8_t slider_02_speed_s2; // from 0 to 100

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
uint8_t remotexy_get_button_01( ) {
// ===========================================================================
  return RemoteXY.button_01;
}

// ===========================================================================
int8_t remotexy_get_slider_01_speed_s1( ) {
// ===========================================================================
  return RemoteXY.slider_01_speed_s1;
}

// ===========================================================================
int16_t remotexy_get_edit_01_s1( ) {
// ===========================================================================
  return RemoteXY.edit_01_s1;
}
 
// ===========================================================================
int16_t remotexy_get_edit_02_s2( ) {
// ===========================================================================
  return RemoteXY.edit_02_s2;
}

// ===========================================================================
int8_t remotexy_get_slider_02_speed_s2( ) {
// ===========================================================================
  return RemoteXY.slider_02_speed_s2;
}

// ===========================================================================
uint8_t remotexy_get_connect_flag( ) {
// ===========================================================================
  return RemoteXY.connect_flag;
}

// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
void remotexy_set_button_01(uint8_t p_button_01 ) {
// ===========================================================================
  RemoteXY.button_01=p_button_01;
}

// ===========================================================================
void remotexy_set_slider_01_speed_s1(int8_t p_slider_01_speed_s1 ) {
// ===========================================================================
  RemoteXY.slider_01_speed_s1=p_slider_01_speed_s1;
}

// ===========================================================================
void remotexy_set_edit_01_s1(int16_t p_edit_01_s1 ) {
// ===========================================================================
  RemoteXY.edit_01_s1=p_edit_01_s1;
}
 
// ===========================================================================
void remotexy_set_edit_02_s2(int16_t p_edit_02_s2 ) {
// ===========================================================================
  RemoteXY.edit_02_s2=p_edit_02_s2;
}

// ===========================================================================
void remotexy_set_slider_02_speed_s2(int8_t p_slider_02_speed_s2 ) {
// ===========================================================================
  RemoteXY.slider_02_speed_s2=p_slider_02_speed_s2;
}

// ===========================================================================
void remotexy_set_connect_flag(uint8_t p_connect_flag ) {
// ===========================================================================
  RemoteXY.connect_flag=p_connect_flag;
}
