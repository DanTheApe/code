# Payload Radiation and Telemetry Logger (ESP32-S3 + SX1280)

This project logs radiation and sensor telemetry on an ESP32-S3, stores it locally, and can send compact telemetry records over LoRa.

## What It Does

- Boots and prints reset reason from both ESP-IDF and ROM/RTC reset sources.
- Starts SPI (SD + LoRa) and I2C (sensors).
- Increments and stores persistent `runId` in Preferences.
- Samples sensors continuously and performs a telemetry cycle every 1 second.
- Writes one CSV row per cycle to SD and/or LittleFS flash (depending on feature flags).

## Telemetry Buffer Model

Telemetry is stored as `id|time|measurement` entries.

- One packet holds up to 10 measurements.
- Ring buffer holds up to 10 packets.
- Total capacity is up to 100 measurements.
- Measurements are written sequentially into the next free slot:
  - packet 1 slot 1 ... packet 1 slot 10
  - packet 2 slot 1 ... and so on
- Measurements with value `0` are skipped (not stored).

## LoRa Send Behavior

- Transmission is attempted only when at least 3 full packets are buffered.
- A LoRa payload is built from buffered packet records (bounded by configured payload size).
- On successful transmit, only the sent packet count is removed from the ring buffer.
- Retry and backoff logic is handled in `LoRaLink`.

## Data and Reliability

- **SD logging**: auto-remount attempts after mount/write failure.
- **Flash logging (LittleFS)**: mirrored logs with quota management.
- **LoRa retries**: counted and tracked as telemetry status.
- **Storage failures**: counted and tracked as telemetry status.

## Serial Commands

Use Serial Monitor at 115200 baud:

- `status` -> print flash/SD status and active paths.
- `print` -> print all flash logs.
- `clear` -> wipe flash logs and recreate current header.
- `new` -> rotate to a new SD file and a new flash file.
- `last` -> dump ring-buffer packets and current pending packet content.

## File Layout

- `src/paylaod.cpp`: entry point (`setup`/`loop`) and shared globals.
- `src/TelemetryApp.cpp`, `include/TelemetryApp.hpp`: app orchestration, buffering, TX flow, serial commands.
- `src/TelemetryRingBuffer.cpp`, `include/TelemetryRingBuffer.hpp`: packet ring buffer.
- `src/GeigerCounter.cpp`, `include/GeigerCounter.hpp`: pulse counting.
- `src/SdLogger.cpp`, `include/SdLogger.hpp`: SD logging + remount support.
- `src/FlashLogger.cpp`, `include/FlashLogger.hpp`: LittleFS logging and quota handling.
- `src/LoRaLink.cpp`, `include/LoRaLink.hpp`: SX1280 setup and transmit/retry logic.
- `include/Config.hpp`: feature flags, pins, constants, and shared declarations.

## Feature Switches

Compile-time flags in `include/Config.hpp`:

- `LoRa`, `SDKort`, `Geiger`, `flashmemori`, `WrightToSerial`
- `IMU_Sensor`, `Baro_Sensor`, `Mag_Sensor`

Set flag to `1` to enable, `0` to disable.