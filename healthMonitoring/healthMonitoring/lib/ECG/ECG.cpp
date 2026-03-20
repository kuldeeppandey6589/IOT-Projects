#include <ECG.hpp>

ECG::ECG(uint8_t analogPin)
{
  this->analogPin = analogPin;
}

void ECG::begin()
{
  pinMode(analogPin, INPUT);
}

float ECG::readBPM()
{
  // Generate a random BPM here for demonstration purposes
  return random(60, 100);
}
