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
  buzzer_soundStartup();

  //neopixels_blinkBoth(CRGB::Green, 3, 20);

 
  button_init();

    // Add battery pin setup
  pinMode(BATTERY_PIN, INPUT);
  
  // Initial connection beep (if starting connected)
  if (remotexy_get_connect_flag()) {
    buzzer_soundOK();  // Single beep for connection
  }
  
  event_connection_state_flag_bo = remotexy_get_connect_flag();  // Set initial state

  g_battery_raw_adc = 0;
  g_battery_voltage = 0;
  g_battery_percentage = 0;

  g_previous_millis_u32 = 0;

  g_speed_slider_s1=90;
  g_speed_slider_s2=90;

  g_speed_edit_01_s1=90;
  g_speed_edit_02_s2=90;

  g_speed_s1 = 90;
  g_speed_s2 = 90;

  g_startup_counter = 10;
}    

// ===========================================================================
void loop() {
// ===========================================================================
  // input variables
  uint8_t l_rxy_button_01;
  
  uint8_t l_rxy_slider_s1; 
  uint8_t l_rxy_slider_s1_mapped;

  uint8_t l_rxy_slider_s2;
  uint8_t l_rxy_slider_s2_mapped;
  
  uint16_t l_rxy_edit_s1; 
  uint16_t l_rxy_edit_s1_constrained; 
  
  uint16_t l_rxy_edit_s2; 
  uint16_t l_rxy_edit_s2_constrained; 
  
  uint8_t l_rxy_button_buzzer; 
  
  uint8_t l_rxy_pushSwitch_led1; 
  uint8_t l_rxy_pushSwitch_led2;
  
  uint8_t l_rxy_rgb_r_1; 
  uint8_t l_rxy_rgb_g_1;
  uint8_t l_rxy_rgb_b_1; 
  
  uint8_t l_rxy_rgb_r_2; 
  uint8_t l_rxy_rgb_g_2; 
  uint8_t l_rxy_rgb_b_2; 

  // output variables
  uint16_t l_rxy_sound_01;
  uint8_t l_rxy_circularBar_speed_m1;
  uint8_t l_rxy_circularBar_speed_m2;
  uint8_t l_rxy_led_button_status; 
  uint16_t l_rxy_value_ultrasonic; 

    // other variable
  uint8_t l_rxy_connect_flag; 
  uint8_t l_rxy_pushSwitch_neopixels;
  
  remotexy_handler();

  // input
  l_rxy_button_01 = remotexy_get_button_01();

  // #########################################################################  
  l_rxy_slider_s1 = remotexy_get_slider_m1();
  l_rxy_slider_s1_mapped = map(l_rxy_slider_s1, 0, 100, 0, 180);

  l_rxy_slider_s2 = remotexy_get_slider_m2();
  l_rxy_slider_s2_mapped = map(l_rxy_slider_s2, 0, 100, 0, 180);

  #if 0
  l_rxy_edit_s1 = remotexy_get_edit_m1();
  l_rxy_edit_s1_constrained = constrain(l_rxy_edit_s1, 0, 180);

  l_rxy_edit_s2 = remotexy_get_edit_m2();
  l_rxy_edit_s2_constrained = constrain(l_rxy_edit_s2, 0, 180);
  #endif
  
  remotexy_set_circularBar_speed_m1(l_rxy_slider_s1_mapped);
  remotexy_set_circularBar_speed_m2(l_rxy_slider_s2_mapped);

  g_speed_s1= l_rxy_slider_s1_mapped;
  g_speed_s2= l_rxy_slider_s2_mapped;

  // ######################################################################### 

  

  l_rxy_button_buzzer = remotexy_get_button_buzzer();
  if( l_rxy_button_buzzer ){
    buzzer_soundOK();    
  }

  l_rxy_pushSwitch_led1 = remotexy_get_pushSwitch_led_1();
  digitalWrite(CONFIG_PIN_LED_GREEN, l_rxy_pushSwitch_led1);

  l_rxy_pushSwitch_led2 = remotexy_get_pushSwitch_led2();
  digitalWrite(CONFIG_PIN_LED_RED, l_rxy_pushSwitch_led2);

  l_rxy_pushSwitch_neopixels =  remotexy_get_pushSwitch_neopixels ();

  if( l_rxy_pushSwitch_neopixels) {
    //neopixels_moving_dot();
    // neopixels_fading_sparkles();

    l_rxy_rgb_b_1 = remotexy_get_rgb_b_1();
    l_rxy_rgb_g_1 = remotexy_get_rgb_g_1();
    l_rxy_rgb_r_1 = remotexy_get_rgb_r_1();

    neopixels_fading_sparkles2(l_rxy_rgb_r_1, l_rxy_rgb_g_1, l_rxy_rgb_b_1);
    //neopixels_soft_rainbow();
    //neopixels_custom_waves();

  // l_rxy_rgb_b_1 = remotexy_get_rgb_b_1();
  // l_rxy_rgb_g_1 = remotexy_get_rgb_g_1();
  // l_rxy_rgb_r_1 = remotexy_get_rgb_r_1();
// 
  // l_rxy_rgb_b_2 = remotexy_get_rgb_b_2();
  // l_rxy_rgb_g_2 = remotexy_get_rgb_g_2();
  // l_rxy_rgb_r_2 = remotexy_get_rgb_r_2();
  // // Apply selected color to all LEDs
  // neopixels_fill( l_rxy_rgb_r_1, l_rxy_rgb_g_1, l_rxy_rgb_b_1,
  //               l_rxy_rgb_r_2, l_rxy_rgb_g_2, l_rxy_rgb_b_2 );
  }else {
    remotexy_set_rgb_b_1(0);
    remotexy_set_rgb_g_1(0);
    remotexy_set_rgb_r_1(0);
    remotexy_set_rgb_b_2(0);
    remotexy_set_rgb_g_2(0);
    remotexy_set_rgb_r_2(0);

    FastLED.clear();
  }

  FastLED.show();


  // output
  l_rxy_sound_01 = remotexy_get_sound_01();
  
  //l_rxy_led_button_status = remotexy_get_led_button_status();
  remotexy_set_led_button_status(digitalRead(CONFIG_PIN_BUTTON));
  
  //l_rxy_value_ultrasonic = remotexy_get_value_ultrasonic();
  remotexy_set_value_ultrasonic(g_ultrasonic_distance_cm);
  // others
  l_rxy_connect_flag = remotexy_get_connect_flag(); 

  


  if (!remotexy_get_connect_flag()) {
    ;
  }

  if(remotexy_get_connect_flag() ) {
    ;  
  }
  else {
    remotexy_set_slider_m1(50);
    remotexy_set_slider_m2(50);
    g_speed_s1= 90;
    g_speed_s2= 90;
  }

  if(remotexy_get_button_01()) {
    g_speed_s1= 90;
    g_speed_s2= 90;

    remotexy_set_slider_m1(50);
    remotexy_set_slider_m2(50);    
    
  }

  if(events_get_connect_flag() == EVENTS_CONNECTION_CONNECTED){
    remotexy_set_slider_m1(50);
    remotexy_set_slider_m2(50);
    g_speed_s1= 90;
    g_speed_s2= 90;
  }  

  if(!g_startup_counter){
    moveServos(g_speed_s1, g_speed_s2);
  }

  tasks_connect();
  tasks_buttons();
  tasks_elapsed_time();
  
 if (events_get_timeout_flag() == EVENTS_TIMEOUT_OCCURED) {  

    if(remotexy_get_connect_flag() ) {
      if( g_startup_counter) {
        g_startup_counter--;
            remotexy_set_slider_m1(50);
    remotexy_set_slider_m2(50);

        g_speed_s1= 90;
        g_speed_s2= 90;
      }    
    }
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
