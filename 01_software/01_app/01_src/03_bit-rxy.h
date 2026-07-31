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

#endif // __DEF_INCLUDE_BIT_RXY_H__