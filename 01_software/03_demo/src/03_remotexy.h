#ifndef __DEF_INCLUDE_REMOTE_XY_H__
#define __DEF_INCLUDE_REMOTE_XY_H__

extern void remotexy_init(void);
extern void remotexy_handler(void);


extern uint8_t remotexy_get_button_01();
extern int8_t remotexy_get_slider_m1();
extern int8_t remotexy_get_slider_m2();
extern int16_t remotexy_get_edit_m1();
extern int16_t remotexy_get_edit_m2();
extern uint8_t remotexy_get_button_buzzer();
extern uint8_t remotexy_get_pushSwitch_led_1();
extern uint8_t remotexy_get_pushSwitch_led2();
extern uint8_t remotexy_get_rgb_b_1();
extern uint8_t remotexy_get_rgb_g_1();
extern uint8_t remotexy_get_rgb_r_1();
extern uint8_t remotexy_get_rgb_b_2();
extern uint8_t remotexy_get_rgb_g_2();
extern uint8_t remotexy_get_rgb_r_2();
extern int16_t remotexy_get_sound_01();
extern int8_t remotexy_get_circularBar_speed_m1();
extern int8_t remotexy_get_circularBar_speed_m2();
extern uint8_t remotexy_get_led_button_status();
extern int16_t remotexy_get_value_ultrasonic();
extern uint8_t remotexy_get_connect_flag();

extern void remotexy_set_button_01(uint8_t p_button_01);
extern void remotexy_set_slider_m1(int8_t p_slider_m1);
extern void remotexy_set_slider_m2(int8_t p_slider_m2);
#if 0
extern void remotexy_set_edit_m1(int16_t p_edit_m1);
extern void remotexy_set_edit_m2(int16_t p_edit_m2);
#endif

extern void remotexy_set_button_buzzer(uint8_t p_button_buzzer);
extern void remotexy_set_pushSwitch_led_1(uint8_t p_pushSwitch_led_1);
extern void remotexy_set_pushSwitch_led2(uint8_t p_pushSwitch_led2);
extern void remotexy_set_rgb_b_1(uint8_t p_rgb_b_1);
extern void remotexy_set_rgb_g_1(uint8_t p_rgb_g_1);
extern void remotexy_set_rgb_r_1(uint8_t p_rgb_r_1);
extern void remotexy_set_rgb_b_2(uint8_t p_rgb_b_2);
extern void remotexy_set_rgb_g_2(uint8_t p_rgb_g_2);
extern void remotexy_set_rgb_r_2(uint8_t p_rgb_r_2);
extern void remotexy_set_sound_01(int16_t p_sound_01);
extern void remotexy_set_circularBar_speed_m1(int8_t p_circularBar_speed_m1);
extern void remotexy_set_circularBar_speed_m2(int8_t p_circularBar_speed_m2);
extern void remotexy_set_led_button_status(uint8_t p_led_button_status);
extern void remotexy_set_value_ultrasonic(int16_t p_value_ultrasonic);
extern void remotexy_set_connect_flag(uint8_t p_connect_flag);

extern uint8_t remotexy_get_pushSwitch_neopixels();
extern void remotexy_set_pushSwitch_neopixels(uint8_t p_pushSwitch_neopixels);

#endif // __DEF_INCLUDE_REMOTE_XY_H__