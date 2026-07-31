#include "01_includes.h"

// ===========================================================================
float readBatteryVoltage() {
// ===========================================================================
  // Read battery voltage (assuming voltage divider)
  int32_t rawValue = analogRead(CONFIG_PIN_BATTERY_LEVEL);
  float voltage = rawValue * (3.3 / 4095.0);  // ESP32 ADC reference voltage is 3.3V
  
  // Adjust for voltage divider ratio if you're using one
  // For example, if using 100k+100k divider:
  //voltage *= 2.0;  // Adjust this based on your actual voltage divider
  voltage *= 3.13;  // Adjust this based on your actual voltage divider
  
  return voltage;
}

// ===========================================================================
int32_t calculateBatteryPercentage(float voltage) {
// ===========================================================================
  // Simple linear calculation - adjust for your battery chemistry
  voltage = constrain(voltage, BATTERY_MIN_V, BATTERY_MAX_V);
  int32_t percentage = (int32_t)((voltage - BATTERY_MIN_V) * 100 / (BATTERY_MAX_V - BATTERY_MIN_V));
  return constrain(percentage, 0, 100);
}
