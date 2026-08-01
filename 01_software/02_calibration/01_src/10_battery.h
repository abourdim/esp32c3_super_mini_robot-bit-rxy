#ifndef __DEF_INCLUDE_BATTERY_H__
#define __DEF_INCLUDE_BATTERY_H__

extern float readBatteryVoltage();
extern int32_t calculateBatteryPercentage(float voltage);

#endif // __DEF_INCLUDE_BATTERY_H__