#ifndef __DEF_INCLUDE_BIT_RXY_H__
#define __DEF_INCLUDE_BIT_RXY_H__

extern void remotexy_init(void);
extern void remotexy_handler(void);

extern int8_t remotexy_get_joystick_01_x( );
extern int8_t remotexy_get_joystick_01_y( );
extern uint8_t remotexy_get_button_01( );
extern float remotexy_get_onlineGraph_01_distance( );
extern float remotexy_get_onlineGraph_02_speed( );
extern float remotexy_get_onlineGraph_03_battery( );
extern int8_t remotexy_get_circularBar_01( );
extern int16_t remotexy_get_sound_01( );
extern uint8_t remotexy_get_connect_flag( );

// ===========================================================================
// ===========================================================================
// ===========================================================================

extern void remotexy_set_joystick_01_x( int8_t p_joystick_01_x);
extern void remotexy_set_joystick_01_y( int8_t p_joystick_01_y);
extern void remotexy_set_button_01(uint8_t p_button_01);
extern void remotexy_set_onlineGraph_01_distance(float p_onlineGraph_01_distance);
extern void remotexy_set_onlineGraph_02_speed( float p_onlineGraph_02_speed);
extern void remotexy_set_onlineGraph_03_battery( float p_onlineGraph_03_battery);
extern void remotexy_set_circularBar_01( int8_t p_circularBar_01);
extern void remotexy_set_sound_01(int16_t p_sound_01);
extern void remotexy_set_connect_flag( uint8_t p_connect_flag);

// ===========================================================================
// Widgets added with the zoned layout. The app owns these values; the rest of
// the firmware only reads them, so they are exposed as accessors rather than
// globals to keep the ownership one-way.
// ===========================================================================

// DRIVE — 0..100 ceiling applied to the drive mix in tasks_joysticks().
extern uint8_t remotexy_get_speed_cap(void);

// LIGHTS — NeoPixel strip. The effect enum is dispatched from loop(), which
// used to call neopixels_waving_french_flag() unconditionally.
enum {
  NP_EFFECT_SOLID = 0,
  NP_EFFECT_RAINBOW,
  NP_EFFECT_KNIGHT,
  NP_EFFECT_DUEL,
  NP_EFFECT_FRENCH
};
extern uint8_t  remotexy_get_np_on(void);
extern uint8_t  remotexy_get_np_effect(void);
extern uint8_t  remotexy_get_np_brightness(void);
extern uint32_t remotexy_get_np_color(void);   // 0x00RRGGBB

// SYSTEM — telemetry verbosity chosen from the Telemetry select.
enum { UPD_OFF = 0, UPD_BASIC, UPD_ALL };
extern uint8_t remotexy_get_upd_level(void);

// SYSTEM — which of the two compiled-in layouts the robot serves. Chosen from
// the Level select, persisted in NVS, defaulting to Beginner on a fresh chip.
// Panels the robot can serve. Beginner/Expert are the two everyday layouts;
// the rest are single-subsystem test panels for classroom bring-up, so a child
// moves between exercises with a dropdown instead of an adult reflashing.
// Order must match LEVEL_OPTIONS and the layout table in 03_bit-rxy.cpp.
enum {
  LAYOUT_BEGINNER = 0,
  LAYOUT_EXPERT,
  LAYOUT_TEST_MOTORS,
  LAYOUT_TEST_DISTANCE,
  LAYOUT_TEST_LIGHTS,
  LAYOUT_TEST_SOUND,
  LAYOUT_TEST_DISPLAY,
  LAYOUT_TEST_POWER,
  LAYOUT_COUNT
};
extern uint8_t remotexy_get_layout_level(void);

// LIGHTS — the two board LEDs. leds_update() owns them until the app takes
// over with a toggle; ownership returns on disconnect. See 14_leds.cpp.
extern uint8_t remotexy_get_led_manual(void);
extern uint8_t remotexy_get_led_r(void);
extern uint8_t remotexy_get_led_g(void);

// Telemetry senders for the widgets that have no RemoteXY-era equivalent.
extern void remotexy_send_graph_distance(float cm);
extern void remotexy_send_obstacle_alert(float cm);
extern void remotexy_send_system_labels(void);
extern void remotexy_send_button_state(void);
extern void remotexy_send_link_rssi(void);
extern void remotexy_send_led_state(void);
// Echoes the Telemetry and Level selectors so they show their real position
// rather than their first option. Not gated on the telemetry level.
extern void remotexy_send_control_echo(void);
// Mirrors the OLED's top line into lbl_oled, and echoes the oled_text field
// after a CFG transfer so it repopulates on reconnect.
extern void remotexy_send_oled_mirror(void);
// Must be called after the senders above, once per telemetry pass — it ends
// the forced full-refresh that follows a CFG transfer.
extern void remotexy_telemetry_end(void);

#endif // __DEF_INCLUDE_BIT_RXY_H__