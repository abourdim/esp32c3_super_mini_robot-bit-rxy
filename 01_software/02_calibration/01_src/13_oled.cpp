#include "01_includes.h"

extern Adafruit_SSD1306 display;

#define SIZE_BUFFER 64
char g_display_array[SIZE_BUFFER];

// ===========================================================================
void oled_init(void) {
// ===========================================================================

}

// ===========================================================================
void oled_update( ) {
// ===========================================================================
int16_t x1, y1;
uint16_t w, h;
  display.clearDisplay();

  // Top: Workshop-DIY credit (small, always visible)
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Workshop-DIY");

  // Line 2: connection status / device name
  display.setTextSize(2);
  display.setCursor(0, 10);

  if( remotexy_get_connect_flag() ) {
    display.print("Calib");
  }else {
    memset(g_display_array, 0,SIZE_BUFFER);
    sprintf(g_display_array, "%s", CONFIG_BLE_DEVICE_NAME);

    display.print(g_display_array);
  }

  // Middle: live L/R pulse readout — the actual calibration readout.
  display.setTextSize(2);

  memset(g_display_array, 0,SIZE_BUFFER);
  sprintf(g_display_array, "L%3u R%3u", g_speed_s1, g_speed_s2);

  display.getTextBounds(g_display_array, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 32);
  display.print(g_display_array);

  // Bottom-right: elapsed time
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
