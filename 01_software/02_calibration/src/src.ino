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

  // remotexy_set_sound_01(REMOTEXY_SOUND_POWER_ON);  
  
  button_init();

    // Add battery pin setup
  pinMode(CONFIG_PIN_BATTERY_LEVEL, INPUT);  
   
  event_connection_state_flag_bo = remotexy_get_connect_flag();  // Set initial state

  g_battery_raw_adc = 0;
  g_battery_voltage = 0;
  g_battery_percentage = 0;

  g_previous_millis_u32 = 0;

  g_speed_slider_s1=CONFIG_SERVO_SPEED_STOP_LEFT;
  g_speed_slider_s2=CONFIG_SERVO_SPEED_STOP_RIGHT;

  g_speed_edit_01_s1=CONFIG_SERVO_SPEED_STOP_LEFT;
  g_speed_edit_02_s2=CONFIG_SERVO_SPEED_STOP_RIGHT;

  g_speed_s1 = CONFIG_SERVO_SPEED_STOP_LEFT;
  g_speed_s2 = CONFIG_SERVO_SPEED_STOP_RIGHT;

}    

// ===========================================================================
void loop() {
// ===========================================================================
  uint8_t l_speed_slider_s1;
  uint8_t l_speed_slider_s2;
  uint8_t l_speed_slider_s1_mapped;
  uint8_t l_speed_slider_s2_mapped;

  uint16_t l_speed_edit_01_s1;
  uint16_t l_speed_edit_02_s2; 
  uint16_t l_speed_edit_01_s1_constrained;
  uint16_t l_speed_edit_02_s2_constrained; 
  
  remotexy_handler();

  if (!remotexy_get_connect_flag()) {
    remotexy_set_edit_01_s1(CONFIG_SERVO_SPEED_STOP_LEFT);
    remotexy_set_edit_02_s2(CONFIG_SERVO_SPEED_STOP_RIGHT);
  }

  if(remotexy_get_connect_flag() ) {

    if(remotexy_get_button_01() ) {
       remotexy_set_edit_01_s1(CONFIG_SERVO_SPEED_STOP_LEFT);
       remotexy_set_edit_02_s2(CONFIG_SERVO_SPEED_STOP_RIGHT);
    }

    l_speed_slider_s1 = remotexy_get_slider_01_speed_s1();
    l_speed_slider_s1_mapped = map(l_speed_slider_s1, 0, 100, 0, 180);

    l_speed_slider_s2 = remotexy_get_slider_02_speed_s2();
    l_speed_slider_s2_mapped = map(l_speed_slider_s2, 0, 100, 0, 180);

    l_speed_edit_01_s1 = remotexy_get_edit_01_s1();
    l_speed_edit_01_s1_constrained = constrain(l_speed_edit_01_s1, 0, 180);

    l_speed_edit_02_s2 = remotexy_get_edit_02_s2();
    l_speed_edit_02_s2_constrained = constrain(l_speed_edit_02_s2, 0, 180);
    
#if 1
    if(l_speed_slider_s1 != g_speed_slider_s1){
      g_speed_slider_s1=l_speed_slider_s1;

      g_speed_s1= l_speed_slider_s1_mapped;
      
      g_speed_edit_01_s1=g_speed_s1;      
      remotexy_set_edit_01_s1(g_speed_s1);
      buzzer_soundButtonPress();
      return;
    }

    if(l_speed_slider_s2 != g_speed_slider_s2){
      g_speed_slider_s2=l_speed_slider_s2;

      g_speed_s2= l_speed_slider_s2_mapped;
      
      g_speed_edit_02_s2=g_speed_s2;      
      remotexy_set_edit_02_s2(g_speed_s2);
      buzzer_soundButtonPress();
      return;
    }
#endif

#if 1
    if(l_speed_edit_01_s1_constrained != g_speed_edit_01_s1){
      g_speed_edit_01_s1=l_speed_edit_01_s1_constrained;
      remotexy_set_edit_01_s1(l_speed_edit_01_s1_constrained);

      g_speed_s1 = l_speed_edit_01_s1_constrained;
      

      l_speed_slider_s1_mapped = map(l_speed_edit_01_s1_constrained, 0, 180, 0, 100);
      g_speed_slider_s1 = l_speed_slider_s1_mapped;
      remotexy_set_slider_01_speed_s1(l_speed_slider_s1_mapped);
      buzzer_soundButtonPress();
      return;
    }

   if(l_speed_edit_02_s2_constrained != g_speed_edit_02_s2){
      g_speed_edit_02_s2=l_speed_edit_02_s2_constrained;
      remotexy_set_edit_02_s2(l_speed_edit_02_s2_constrained);

      g_speed_s2 = l_speed_edit_02_s2_constrained;
      

      l_speed_slider_s2_mapped = map(l_speed_edit_02_s2_constrained, 0, 180, 0, 100);
      g_speed_slider_s2 = l_speed_slider_s2_mapped;
      remotexy_set_slider_02_speed_s2(l_speed_slider_s2_mapped);
      buzzer_soundButtonPress();
      return;
    }
#endif

  }else {
    g_speed_s1= CONFIG_SERVO_SPEED_STOP_LEFT;;
    g_speed_s2= CONFIG_SERVO_SPEED_STOP_RIGHT;
  }

  moveServos(g_speed_s1, g_speed_s2);

  tasks_connect();
  tasks_buttons();
  tasks_elapsed_time();
  
 if (events_get_timeout_flag() == EVENTS_TIMEOUT_OCCURED) {  
    
    ultrasonic_get_distance();
    tasks_battery();
    
    // tasks_remotexy();
    oled_update();
    leds_update();

  }

  events_reset_connect_flag();

  if (events_get_timeout_flag() == EVENTS_TIMEOUT_OCCURED) {
    events_reset_timeout_flag();
  }
  
}
