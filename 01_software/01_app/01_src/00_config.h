#ifndef __DEF_INCLUDE_CONFIG_H__
#define __DEF_INCLUDE_CONFIG_H__


#define CONFIG_BLE_DEVICE_NAME "diy_app_b3"

#define DEF_DERIAL_DEBUG



// --------------------------------------
// button
#define CONFIG_PIN_BUTTON 0

// --------------------------------------
// buzzer
#define CONFIG_PIN_BUZZER 4
#define CONFIG_BUZZER_FREQ 4000

// battery level
#define CONFIG_PIN_BATTERY_LEVEL 4

#define BATTERY_MIN_V 3.3  // Minimum battery voltage (adjust according to your battery)
//#define BATTERY_MAX_V 4.2  // Maximum battery voltage (adjust according to your battery)
#define BATTERY_MAX_V 4.5  // Maximum battery voltage (adjust according to your battery)

// --------------------------------------
// neopixel

#define CONFIG_NEOPIXELS_MAX_LEDS 256      // Max supported (array size)

#if 1
  #define CONFIG_PIN_NEOPIXEL 5
  #define CONFIG_NEOPIXELS_NB_LEDS 4       // Active LED count
#else
  #define CONFIG_PIN_NEOPIXEL 10            // external strip
  #define CONFIG_NEOPIXELS_NB_LEDS 16       // Active LED count
#endif

#define CONFIG_NEOPIXELS_BRIGHTNESS 15       // Brightness (0-255)



// --------------------------------------
// I2C pins
#define CONFIG_PIN_OLED_SDA 8
#define CONFIG_PIN_OLED_SCL 9

// --------------------------------------
// Ultrasonic sensor pins
#define CONFIG_PIN_SR04_TRIG 21
#define CONFIG_PIN_SR04_ECHO 20

// --------------------------------------
// Servo configuration

#define CONFIG_SERVO_SPEED_STOP_LEFT    90
#define CONFIG_SERVO_SPEED_STOP_RIGHT   90

#define CONFIG_SERVO_SPEED_STOP_LEFT_OFFSET    5
#define CONFIG_SERVO_SPEED_STOP_RIGHT_OFFSET    5

#define CONFIG_PIN_SERVO_LEFT  6   // GPIO 6 for left servo
#define CONFIG_PIN_SERVO_RIGHT 3   // GPIO 3 for right servo

// #define CONFIG_PIN_SERVO_LEFT  7   // GPIO 6 for left servo
// #define CONFIG_PIN_SERVO_RIGHT 2   // GPIO 3 for right servo

// #define CONFIG_PIN_SERVO_LEFT  10   // GPIO 6 for left servo
// #define CONFIG_PIN_SERVO_RIGHT 1   // GPIO 3 for right servo

#define CONFIG_PIN_LED_RED 10   //
#define CONFIG_PIN_LED_GREEN 1   //

// #define LED_PIN_1 2
// #define LED_PIN_2 6

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // Reset pin not used

#endif // __DEF_INCLUDE_CONFIG_H__
