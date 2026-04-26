# Payload Telemetry Logger (ESP32-S3 + SX1280)

This project logs sensor telemetry on an ESP32-S3, stores it locally, and
transmits it over the SX1280 2.4 GHz LoRa radio.

The repository contains **two PlatformIO build environments** that share the
same `include/` and `src/` trees but compile different sources:

| Environment   | Source filter        | Description |
|---------------|----------------------|-------------|
| `main`        | `+<*> -<tests/*>`    | Original CSV-based logger using a packet ring buffer of `id|t|value` records and a multi-message MAVLink frame per cycle. |
| `new`         | `+<New/*>`           | Rewritten firmware. One MAVLink message per sensor reading, BUSY-pin TX, ring-buffered LoRa retries, key=value labelled log rows. |
| `test_*`      | one test file        | Stand-alone hardware sanity checks for each sensor / radio. |

Build any environment with `pio run -e <name>`, upload with `-t upload`.

## Hardware

- ESP32-S3-WROOM-1U-N16R2 (16 MB flash, 2 MB PSRAM)
- ICM-45686 IMU (SPI, CS = GPIO16)
- BMP388 barometer (I2C, 0x76)
- BMM350 magnetometer (I2C, 0x14)
- SX1280 LoRa transceiver (SPI, CS = GPIO9, BUSY = GPIO13)
- Optional: TMP1075 (0x73) and DAC43401 (0x72) next to the LoRa module
- Optional: microSD card on PIN_SD_CS = GPIO21
- Optional: dual Geiger inputs on GPIO8 and GPIO3

Full pin map: see "Main board testing.pdf" in the repo root or
[`include/New/Config.hpp`](include/New/Config.hpp).

## Choose a build

- New work / from-scratch reading → start with the **`new`** environment
  and read [`New_README.md`](New_README.md).
- Existing CSV-row format / ring-of-packets behaviour → use the **`main`**
  environment (described below).

## `main` environment — telemetry buffer model

Telemetry is stored as `id|time|measurement` entries.

- One packet holds up to 10 measurements.
- Ring buffer holds up to 10 packets.
- Total capacity is up to 100 measurements.
- Measurements are written sequentially into the next free slot.
- Measurements with value `0` are skipped (not stored).

### LoRa Send Behavior

- Transmission is attempted only when at least 3 full packets are buffered.
- A LoRa payload is built from buffered packet records (bounded by configured
  payload size).
- On successful transmit, only the sent packet count is removed from the ring
  buffer.
- Retry and backoff logic is handled in `LoRaLink`.

### Data and Reliability

- **SD logging**: auto-remount attempts after mount/write failure.
- **Flash logging (LittleFS)**: mirrored logs with quota management.
- **LoRa retries**: counted and tracked as telemetry status.
- **Storage failures**: counted and tracked as telemetry status.

### Serial Commands

Use Serial Monitor at 115200 baud:

- `status` — print flash/SD status and active paths.
- `print`  — print all flash logs.
- `clear`  — wipe flash logs and recreate current header.
- `new`    — rotate to a new SD file and a new flash file.
- `last`   — dump ring-buffer packets and current pending packet content.

### File Layout (`main` env)

- `src/paylaod.cpp`: entry point (`setup`/`loop`) and shared globals.
- `src/TelemetryApp.cpp`, `include/TelemetryApp.hpp`: app orchestration,
  buffering, TX flow, serial commands.
- `src/TelemetryRingBuffer.cpp`, `include/TelemetryRingBuffer.hpp`: packet
  ring buffer.
- `src/GeigerCounter.cpp`, `include/GeigerCounter.hpp`: pulse counting.
- `src/SdLogger.cpp`, `include/SdLogger.hpp`: SD logging + remount support.
- `src/FlashLogger.cpp`, `include/FlashLogger.hpp`: LittleFS logging and
  quota handling.
- `src/LoRaLink.cpp`, `include/LoRaLink.hpp`: SX1280 setup and transmit/retry
  logic.
- `include/Config.hpp`: feature flags, pins, constants, shared declarations.

### Feature Switches (`main` env)

Compile-time flags in `include/Config.hpp`:

- `LoRa`, `SDKort`, `Geiger`, `flashmemori`, `WrightToSerial`
- `IMU_Sensor`, `Baro_Sensor`, `Mag_Sensor`

Set flag to `1` to enable, `0` to disable.

## Component tests

Each `test_*` env builds a single self-contained test that exercises one
peripheral and prints PASS/FAIL on the serial monitor:

```bash
pio run -e test_imu     -t upload && pio device monitor -e test_imu
pio run -e test_baro    -t upload && pio device monitor -e test_baro
pio run -e test_mag     -t upload && pio device monitor -e test_mag
pio run -e test_lora    -t upload && pio device monitor -e test_lora
pio run -e test_geiger  -t upload && pio device monitor -e test_geiger
pio run -e test_sdcard  -t upload && pio device monitor -e test_sdcard
pio run -e test_flash   -t upload && pio device monitor -e test_flash
```

These are useful for isolating hardware faults before debugging firmware.
