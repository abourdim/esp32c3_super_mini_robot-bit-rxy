#ifndef __DEF_INCLUDE_REMOTE_XY_H__
#define __DEF_INCLUDE_REMOTE_XY_H__

extern void remotexy_init(void);
extern void remotexy_handler(void);


extern uint8_t remotexy_get_button_01( );
extern int8_t remotexy_get_slider_01_speed_s1( );
extern int16_t remotexy_get_edit_01_s1( );
extern int16_t remotexy_get_edit_02_s2( );
extern int8_t remotexy_get_slider_02_speed_s2( );
extern uint8_t remotexy_get_connect_flag( );

// ===========================================================================
// ===========================================================================
// ===========================================================================

extern void remotexy_set_button_01(uint8_t p_button_01 );
extern void remotexy_set_slider_01_speed_s1(int8_t p_slider_01_speed_s1 );
extern void remotexy_set_edit_01_s1(int16_t p_edit_01_s1 );
extern void remotexy_set_edit_02_s2(int16_t p_edit_02_s2 );
extern void remotexy_set_slider_02_speed_s2(int8_t p_slider_02_speed_s2 );
extern void remotexy_set_connect_flag(uint8_t p_connect_flag );

#endif // __DEF_INCLUDE_REMOTE_XY_H__