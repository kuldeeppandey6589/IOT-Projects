# Transformer Health Monitoring (Oil) — NodeMCU

ESP8266 NodeMCU firmware for monitoring transformer oil using:

- LM35 temperature sensor (A0)
- HC-SR04 ultrasonic for oil level (D6 trig, D7 echo)
- I2C 16x2 LCD via Robotutor-tech/LCD library
- Optional cooling fan/relay (D4)
- Buzzer alarm (D0)
- Publishes metrics to Adafruit IO (update AIO credentials)

Features

- Displays Temp (°C) and Level (%) on LCD
- Alarms for overheating (>60°C) and low oil (<25%)
- MQTT feeds: oil-temp, oil-level, alarm (0=ok,1=warn,2=critical)

Wiring

- LM35 OUT -> A0 (ensure max 1.0V to ESP8266; use divider if needed)
- HC-SR04: TRIG -> D6, ECHO -> D7 (use 5V->3.3V divider on ECHO if sensor is 5V)
- I2C LCD: SDA -> D2, SCL -> D1, VCC 5V, GND
- Buzzer -> D0
- Fan/Relay (active-low typical) -> D4

Config

- Edit `platformio.ini` board/env if needed
- In `src/main.cpp`, update:
  - WLAN_SSID, WLAN_PASS
  - AIO_USERNAME, AIO_KEY
  - OIL_TANK_HEIGHT_CM, SENSOR_TOP_CLEARANCE_CM

Build/Upload

- Use PlatformIO: select env `nodemcuv2`, upload, open serial @115200.

Notes

- LM35 scaling assumes 0–1.0V ADC range; ensure proper conditioning.
- Level math assumes sensor mounted at the top; tune height/clearance.
