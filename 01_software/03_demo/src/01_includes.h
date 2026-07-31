#ifndef __DEF_INCLUDE_INCLUDES_H__
#define __DEF_INCLUDE_INCLUDES_H__

#include <Arduino.h>  // Essential for Arduino functions
#include <FastLED.h>
#include <ESP32Servo.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <driver/adc.h>

#include "00_config.h"
// #include "01_includes.h"
#include "02_data.h"
#include "03_remotexy.h"
#include "04_tasks.h"
#include "05_buzzer.h"
#include "06_neopixels.h"
#include "07_sounds.h"
#include "08_button.h"
#include "09_servos.h"
#include "10_battery.h"
#include "11_events.h"
#include "12_ultrasonic.h"
#include "13_oled.h"
#include "14_leds.h"

uint8_t remotexy_get_connect_flag( );

#endif // __DEF_INCLUDE_INCLUDES_H__