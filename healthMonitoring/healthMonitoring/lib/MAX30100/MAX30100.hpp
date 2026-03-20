#ifndef MAX30100_WRAPPER_HPP
#define MAX30100_WRAPPER_HPP

#include <Arduino.h>

class MAX30100Wrapper
{
public:
  MAX30100Wrapper();
  void begin();
  float getHeartRate();
  float getSpO2();
};

#endif
