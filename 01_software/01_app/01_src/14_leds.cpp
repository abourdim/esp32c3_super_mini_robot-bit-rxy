#include "01_includes.h"

#define LED_PIN_1 2
#define LED_PIN_2 6

#define PWM_CHANNEL_1 0
#define PWM_CHANNEL_2 1

#define PWM_FREQ 5000        // 5 kHz frequency
#define PWM_RESOLUTION 8     // 8-bit resolution (0-255)

bool g_leds_toggle;
int dutyCycle;
// ===========================================================================
void leds_init(void) {
// ===========================================================================
  #if 1
  pinMode(CONFIG_PIN_LED_RED, OUTPUT);
  digitalWrite(CONFIG_PIN_LED_RED, LOW);

  pinMode(CONFIG_PIN_LED_GREEN, OUTPUT);
  digitalWrite(CONFIG_PIN_LED_GREEN, LOW); 
  #endif

  // Set up both PWM channels
  // ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
  // ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);

  g_leds_toggle=false;
}

// ===========================================================================
void leds_update(void) {
// ===========================================================================
  
  g_leds_toggle=!g_leds_toggle;
  dutyCycle++;

  // The app can take these two LEDs over from the LIGHTS zone. Until it does,
  // they stay the link-status indicator below. Ownership is dropped again on
  // disconnect (see onDisconnect), so "not connected" is always visible even
  // if the user left them switched off.
  if( remotexy_get_led_manual() ) {
    digitalWrite(CONFIG_PIN_LED_RED,   remotexy_get_led_r());
    digitalWrite(CONFIG_PIN_LED_GREEN, remotexy_get_led_g());
    if(g_leds_toggle) {
      remotexy_set_sound_01(0);
    }
    return;
  }

  if( remotexy_get_connect_flag() ) {
    digitalWrite(CONFIG_PIN_LED_GREEN, g_leds_toggle);
    digitalWrite(CONFIG_PIN_LED_RED, 0);

    // ledcWrite(PWM_CHANNEL_1, dutyCycle);  // LED on GPIO 2

  }else {
    digitalWrite(CONFIG_PIN_LED_GREEN, 0);
    digitalWrite(CONFIG_PIN_LED_RED, g_leds_toggle);

    // ledcWrite(PWM_CHANNEL_2, dutyCycle);  // LED on GPIO 2
  }

  if(g_leds_toggle) {
    remotexy_set_sound_01(0);
  }

}
