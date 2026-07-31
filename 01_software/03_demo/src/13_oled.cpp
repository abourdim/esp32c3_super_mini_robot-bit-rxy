#include "01_includes.h"

extern Adafruit_SSD1306 display;

#define SIZE_BUFFER 64
char g_display_array[SIZE_BUFFER];

// Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

// ===========================================================================
void oled_init(void) {
// ===========================================================================

#if 0
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

  #endif
}

// ===========================================================================
void oled_update( ) {
// ===========================================================================
int16_t x1, y1;
uint16_t w, h;
uint8_t l_count;
static uint8_t s_toggle;

  // Update OLED display
  display.clearDisplay();

  // Line 1: Workshop info
  display.setTextSize(2);
  display.setCursor(0, 0);
  
  if( remotexy_get_connect_flag() ) {
    display.print(" Workshop");
  }else {
    memset(g_display_array, 0,SIZE_BUFFER);  
    sprintf(g_display_array, "%s", CONFIG_REMOTEXY_BLUETOOTH_NAME);
    
    display.print(g_display_array);
  }

  // Line 3: Centered distance
  display.setTextSize(2);

  memset(g_display_array, 0,SIZE_BUFFER);

  
  if(s_toggle){    
    sprintf( g_display_array, "%03u %03u", g_speed_s2, g_speed_s1);
  }else {
    sprintf( g_display_array, "%03u cm", g_ultrasonic_distance_cm);
  }
  if(s_toggle) {
    s_toggle--;
  }else {
    s_toggle=10;
  }


  display.getTextBounds(g_display_array, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 32);
  display.print(g_display_array);

  // Bottom-right: time
  display.setTextSize(1);    

  memset(g_display_array, 0,SIZE_BUFFER);
  sprintf(g_display_array, "%02u:%02u:%02u", g_elapsed_time_hours, g_elapsed_time_minutes, g_elapsed_time_seconds);
  
  display.getTextBounds(g_display_array, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w - 2, SCREEN_HEIGHT - h);
  display.print(g_display_array);

  // Battery indicator (bottom-left)
  display.setTextSize(1);
  display.setCursor(0, SCREEN_HEIGHT - h);
  display.print(String(g_battery_voltage, 1) + "V (" + String((int32_t)calculateBatteryPercentage(g_battery_voltage)) + "%)");

  display.display();

}
