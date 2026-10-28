#ifndef ECG_SENSOR_HPP
#define ECG_SENSOR_HPP

#include <Arduino.h>

class ECG
{
private:
  uint8_t analogPin;

public:
  ECG(uint8_t analogPin);
  void begin();
  float readVoltage();
  float readBPM();
};

#endif
