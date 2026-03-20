#include <MAX30100.hpp>

// NOTE: This is a minimal stub wrapper. If you have an actual MAX30100 library
// in PlatformIO, replace this stub with calls to that library's API.

MAX30100Wrapper::MAX30100Wrapper() {}

void MAX30100Wrapper::begin()
{
  // initialize any hardware or defaults
}

float MAX30100Wrapper::getHeartRate()
{
  // Generate a random HeartRate here for demonstration purposes
  // Or write a logic to read from the actual sensor

  return random(60, 100);
}

float MAX30100Wrapper::getSpO2()
{
  // Generate a random SpO2 here for demonstration purposes
  return random(90, 100);
}
