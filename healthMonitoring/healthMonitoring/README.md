# HealthMonitoring

This project is a PlatformIO/Arduino based IoT health monitoring sample. It follows the coding style used in the `patientCareMonitoringSystem` project: small hardware wrapper classes in `lib/*`, a simple `LCD` wrapper, and an MQTT-based data publisher in `src/main.cpp`.

Use cases covered by this scaffold:

- Presence/flux sensors: `water`, `food`, `help` (digital inputs)
- ECG sensor (analog input)
- MAX30100 (heart rate & SpO2) wrapper (stub implementation)
- DHT11 (temperature & humidity)

How to use:

1. Update `src/main.cpp` with your WiFi credentials and Adafruit IO AIO key (set `AIO_KEY`).
2. Connect sensors to the pins referenced in `src/main.cpp` or change pins there.
3. Build and upload using PlatformIO:

```bash
# From project root
platformio run --target upload
platformio device monitor -b 115200
```

Notes:

- The `MAX30100Wrapper` and `ECG` classes are stubs and provide fake values. Replace with a real sensor library or algorithm for production use.
- The project uses `LiquidCrystal_I2C`, `Adafruit MQTT Library`, and `DHT-sensor-library` via `platformio.ini` `lib_deps`. Add more libraries if needed.

Files added/updated:

- `lib/FluxSensor/*` - Digital flux sensor wrapper
- `lib/DHT11/*` - DHT11 wrapper using `DHT.h`
- `lib/ECG/*` - ECG analog wrapper (stub)
- `lib/MAX30100/*` - MAX30100 wrapper (stub)
- `lib/LCD/*` - LCD wrapper copied from patientCareMonitoringSystem
- `src/main.cpp` - main program and MQTT publishing
- `platformio.ini` - dependencies and monitor speed

Replace and expand sensor code with actual algorithms/libraries for production or testing of the hardware. This scaffold follows the class and structure style of `patientCareMonitoringSystem` for consistency.
