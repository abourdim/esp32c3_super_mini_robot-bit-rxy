#ifndef __DEF_INCLUDE_BIT_RXY_H__
#define __DEF_INCLUDE_BIT_RXY_H__

extern void remotexy_init(void);
extern void remotexy_handler(void);

extern int16_t remotexy_get_pulse_left( );
extern int16_t remotexy_get_pulse_right( );
extern uint8_t remotexy_get_connect_flag( );

#endif // __DEF_INCLUDE_BIT_RXY_H__
