# Payload Radiation & Telemetry Logger (ESP32-S3 + SX1280)

This project logs radiation and sensor telemetry on an ESP32, stores it locally, and optionally sends compact packets over LoRa using MavLink protocall.

## What it does

- Boots and reads reset reason.
- Starts SPI (for SD + LoRa) and I2C (for sensors).
- Increments and stores a persistent `runId` in Preferences.
- Every 6 seconds:
  - Reads Geiger pulse count (counts in the last window).
  - Reads sensors (IMU, barometer, magnetometer) if enabled.
  - Pushes sample into a ring buffer.
  - If at least 3 samples are buffered, builds one compact LoRa payload and transmits with retry/backoff.
  - Writes a CSV row to SD and/or LittleFS flash (based on feature flags).

## Data and reliability behavior

- **SD logging**: tries to remount automatically if card write/mount fails.
- **Flash logging (LittleFS)**: writes mirror logs with a quota limit; stops writing when quota is reached.
- **LoRa retries**: up to configured max retries with exponential backoff + jitter.
- **Status counters**: tracks total LoRa retries and storage failures and includes them in logs.

## Serial commands

Use the serial monitor (115200 baud):

- `status` -> print flash/SD status and active log file paths.
- `print` -> print all flash logs to serial.
- `clear` -> wipe flash log directory and recreate current file header.
- `new` -> rotate to a new SD file and new flash file.

## File layout

- `paylaod.cpp`: thin entry point (`setup`/`loop`) and shared globals.
- `TelemetryApp.*`: main orchestration logic.
- `GeigerCounter.*`: interrupt pulse counting.
- `SdLogger.*`: SD CSV logging and remount logic.
- `FlashLogger.*`: LittleFS logging, quota, recursive print/delete.
- `LoRaLink.*`: SX1280 setup and transmit-retry wrapper.
- `TelemetryRingBuffer.*`: fixed-capacity sample buffer.
- `Config.hpp`: feature flags, pins, constants, shared declarations.

## Feature switches

Compile-time flags in `Config.hpp` control modules:

- `LoRa`, `SDKort`, `Geiger`, `flashmemori`, `WrightToSerial`
- `IMU_Sensor`, `Baro_Sensor`, `Mag_Sensor`

Set a flag to `1` to enable, `0` to disable.

## AI
- This README file was writen using AI.