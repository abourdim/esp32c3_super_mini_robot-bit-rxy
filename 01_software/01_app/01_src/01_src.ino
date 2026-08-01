///////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

#include "01_includes.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===========================================================================
void setup() {
// ===========================================================================

  // Serial for debugging
  #ifdef DEF_DERIAL_DEBUG
  Serial.begin(115200);
  #endif

  // Initialize RemoteXY
  //RemoteXY_Init();

  remotexy_init();
  
  // Allocate timer for ESP32 PWM
  ESP32PWM::allocateTimer(0);

  // Attach servos to their pins
  servoLeft.attach(CONFIG_PIN_SERVO_LEFT);
  servoRight.attach(CONFIG_PIN_SERVO_RIGHT);

  // Stop both servos initially
  stopServos();

    // Set custom I2C pins
  Wire.begin(CONFIG_PIN_OLED_SDA, CONFIG_PIN_OLED_SCL);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    #ifdef DEF_DERIAL_DEBUG
    Serial.println(F("SSD1306 allocation failed"));
    #endif
    // for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  leds_init();
  oled_init();

  ultrasonic_init();

  g_ultrasonic_distance_cm=0;
  g_elapsed_time_startup_millis = millis();  // Record start time

  neopixels_init();

  buzzer_init();
  buzzer_beep();

  neopixels_all_blink(CRGB::Green, 3, 20);

  remotexy_set_sound_01(REMOTEXY_SOUND_POWER_ON);  
  
  button_init();

  // Hold the debug button for CONFIG_OTA_HOLD_MS right here (after boot,
  // never during reset — GPIO0 is also the BOOT strapping pin) to enter
  // WiFi OTA mode instead of the normal BLE-driven robot. Blocks forever
  // if entered; only returns if the button wasn't held.
  ota_maybe_enter();

    // Add battery pin setup
  pinMode(CONFIG_PIN_BATTERY_LEVEL, INPUT);  
   
  event_connection_state_flag_bo = remotexy_get_connect_flag();  // Set initial state

  g_battery_raw_adc = 0;
  g_battery_voltage = 0;
  g_battery_percentage = 0;

  g_previous_millis_u32 = 0;

}

// ===========================================================================
void loop() {
// ===========================================================================

  remotexy_handler();

  tasks_connect();
  tasks_buttons();
  tasks_joysticks();
  tasks_elapsed_time();
  tasks_rmotexy_sound();

  if (events_get_timeout_flag() == EVENTS_TIMEOUT_OCCURED) {

    ultrasonic_get_distance();
    tasks_battery();

    tasks_remotexy();
    oled_update();
    leds_update();

  }

  events_reset_connect_flag();

  if (events_get_timeout_flag() == EVENTS_TIMEOUT_OCCURED) {
    events_reset_timeout_flag();
  }

  //neopixels_star();

  neopixels_waving_french_flag();

  //neopixels_KnightRiderRedEyewithTail();
  //neopixels_KnightRiderRedEyewithTail2();
  //neopixels_duel_eye();

  //neopixels_animate();
}
