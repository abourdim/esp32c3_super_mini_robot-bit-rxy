#include "01_includes.h"

extern Adafruit_SSD1306 display;

#define SIZE_BUFFER 64
char g_display_array[SIZE_BUFFER];

// Top-line override sent by the app (SET oled_text). 21 characters is what
// fits across 128px at text size 1, plus a terminator; anything longer is
// truncated here rather than silently running off the right edge.
#define OLED_TEXT_MAX 21
static char s_oled_text[OLED_TEXT_MAX + 1] = {0};

bool        oled_text_active(void) { return s_oled_text[0] != '\0'; }
const char* oled_text_get(void)    { return s_oled_text; }

void oled_text_set(const char* s) {
  if (s == nullptr) { s_oled_text[0] = '\0'; return; }
  strncpy(s_oled_text, s, OLED_TEXT_MAX);
  s_oled_text[OLED_TEXT_MAX] = '\0';
}

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
  // Update OLED display
  display.clearDisplay();

  // Line 1: whatever the app has sent, otherwise the usual banner.
  // Custom text is drawn at size 1 rather than 2 — the app can send far more
  // characters than "Workshop.3", and at size 2 anything past ~10 would run
  // off the panel silently. The rest of the screen (distance, uptime,
  // battery) is untouched either way.
  display.setCursor(0, 0);

  if( oled_text_active() ) {
    display.setTextSize(1);
    display.print(oled_text_get());
  } else if( remotexy_get_connect_flag() ) {
    display.setTextSize(2);
    display.print("Workshop.3");
  } else {
    display.setTextSize(2);
    memset(g_display_array, 0,SIZE_BUFFER);
    sprintf(g_display_array, "%s", CONFIG_BLE_DEVICE_NAME);

    display.print(g_display_array);
  }

  // Line 3: Centered distance
  display.setTextSize(2);

  memset(g_display_array, 0,SIZE_BUFFER);
  sprintf( g_display_array, "%lu cm", g_ultrasonic_distance_cm);
  //sprintf( g_display_array, "%3u %3u %u", g_speed_s2, g_speed_s1);

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
