# Payload Telemetry — `new` environment

Rewritten payload firmware. Sensors are polled at 20 Hz, every 1 s a TX cycle
runs that:

1. drains the LoRa retry queue,
2. reads each sensor and packs it into its own MAVLink message,
3. transmits each MAVLink frame as a separate LoRa packet,
4. appends one labelled `key = value | ...` row to LittleFS (and SD if
   present),
5. processes any serial commands.

Build and flash:

```bash
pio run -e new -t upload
pio device monitor -e new
```

## What goes on the air

One MAVLink message per sensor reading; one LoRa packet per MAVLink message.
Field types match MAVLink wire types so no extra serialisation is needed.

| Sensor reading        | MAVLink message      | Payload fields used         | Wire type             |
|-----------------------|----------------------|-----------------------------|-----------------------|
| Heartbeat (5 s)       | `HEARTBEAT`          | `MAV_STATE_ACTIVE`          | —                     |
| Accelerometer x/y/z   | `SCALED_IMU`         | `xacc / yacc / zacc`        | `int16_t` mg          |
| Gyroscope x/y/z       | `SCALED_IMU2`        | `xgyro / ygyro / zgyro`     | `int16_t` mrad/s      |
| Magnetometer x/y/z    | `SCALED_IMU3`        | `xmag / ymag / zmag`        | `int16_t` mgauss      |
| Barometer pressure    | `SCALED_PRESSURE`    | `press_abs`                 | `float` hPa           |
| Barometer temperature | `NAMED_VALUE_INT`    | name `BARO_T`               | `int32_t` cdegC       |
| Geiger CRD1 / CRD2    | `NAMED_VALUE_INT`    | name `GEIG_1` / `GEIG_2`    | `int32_t` counts      |
| LoRa-module temp      | `NAMED_VALUE_INT`    | name `LORA_T`               | `int32_t` cdegC       |
| LoRa DAC voltage      | `NAMED_VALUE_INT`    | name `LORA_V`               | `int32_t` mV          |

For the multi-axis frames, the MAVLink fields the reading does *not*
populate (e.g. gyro/mag/temp inside `SCALED_IMU`) are zero-filled — only the
axes that belong to that sensor carry data.

## LoRa retry strategy

`LoraTx::send()` makes one immediate transmit attempt. On failure the frame
is pushed onto a ring buffer (`LORA_RETRY_QUEUE = 16` entries). Every loop
tick `LoraTx::service(2)` retries up to two queued frames. When a frame's
attempt count hits `LORA_MAX_ATTEMPTS = 3` it is dropped and counted.

This decouples retries from the 1 s TX cycle: a transient outage queues a
few frames and they catch up later, without delaying fresh data.

## TX path quirk: BUSY-pin polling

The blocking `radio.transmit()` call in RadioLib waits for **DIO1** to go
HIGH on TxDone. On this board DIO1 (CXT, GPIO11) does not assert HIGH on
TxDone — `test_lora` confirms it stays LOW. So this firmware uses
`startTransmit()` followed by polling the **BUSY pin** (GPIO13) until it
goes LOW (with a 100 ms ceiling), then calls `standby()` to reset state.
That mirrors what `test_lora` does and avoids the spurious
`RADIOLIB_ERR_TX_TIMEOUT` (-5) that would otherwise be returned for every
otherwise-successful transmission.

## Local logging

Each TX cycle also produces one self-labelled row written to LittleFS
(and SD if present), e.g.:

```
Run_ID = 33|Now_Ms = 12345|Accsloration_mg = 41,-693,731|Gyro_mrad_s = -3,2,4|Mag_mgauss = 248,-52,-730|Pressure_hPa = 1002.29|Temperature_cC = 2927|Geiger_1 = 0|Geiger_2 = 0|LoRa_Temp_cC = 0|LoRa_Volt_mV = 0|LoRa_Retries = 0|LoRa_Drops = 0|Storage_Fails = 0
```

Format is `key = value | key = value | ...` — fields are self-describing,
so a header row is no longer required. Every flash file is named
`/logs/run_NNNNNN.csv` where `NNNNNN` is the run id from NVS (incremented
on every cold boot).

## Serial monitor — what gets printed

At 115200 baud you'll see, per cycle:

```
[TX] HEARTBEAT  (no payload)  (12 bytes)        ← only every HEARTBEAT_INTERVAL_MS
[TX] ACCEL    t=12345 ms  x=41 y=-693 z=731 mg  (36 bytes)
[TX] GYRO     t=12345 ms  x=-3 y=2 z=4 mrad/s   (36 bytes)
[TX] MAG      t=12345 ms  x=248 y=-52 z=-730 mgauss  (36 bytes)
[TX] BARO_P   t=12345 ms  press=1002.29 hPa     (28 bytes)
[TX] BARO_T   t=12345 ms  temp=2927 cC          (30 bytes)
[TX] GEIG_1   t=12345 ms  counts=0              (30 bytes)
[TX] GEIG_2   t=12345 ms  counts=0              (30 bytes)
[ROW] Run_ID = 33|...
[LoRa] sent=24 retries=0 drops=0 queued=0
```

A failed transmit prints its RadioLib error code:

```
[LoRa] transmit() failed: -5 (len=21)
[LoRa] retry transmit() failed: -5 (attempt=2, len=21)
[LoRa] dropped frame after 3 attempts
```

## Serial commands

Type a command + Enter at 115200 baud:

| Command   | Effect |
|-----------|--------|
| `status`  | print SD/flash mount state, run id, LoRa counters. |
| `print`   | dump every flash log file to serial. |
| `clear`   | wipe all flash logs and recreate the current file. |
| `new`     | bump run id (rotates to a new flash log file). |
| `last`    | print the most recent cached accel/gyro/mag/baro values. |

## Configuration switches — `include/New/Config.hpp`

```cpp
#define USE_SERIAL     1   // print status/debug to USB CDC
#define PRINT_TX       1   // [TX] line per MAVLink frame
#define PRINT_TX_HEX   0   // also dump raw frame bytes (hex)
#define USE_LORA       1   // SX1280 transmitter + MAVLink frames
#define USE_SD         0   // microSD card on PIN_SD_CS
#define USE_FLASH      1   // LittleFS log mirror in onboard flash
#define USE_GEIGER     1   // dual Geiger pulse counter
#define USE_IMU        1   // ICM-45686 accel + gyro
#define USE_BARO       1   // BMP388 barometer
#define USE_MAG        1   // BMM350 magnetometer
#define USE_LORA_TEMP  1   // TMP1075 next to LoRa module
#define USE_LORA_VOLT  1   // DAC43401 setting LoRa TX power
```

Setting any sensor flag to `0` removes it from the build entirely (the
`if (a.ok)` checks ensure the corresponding MAVLink frame is not sent and
the CSV row reports `0`).

LoRa radio settings (`LORA_FREQ_MHZ`, `LORA_BW_KHZ`, `LORA_SF`, `LORA_CR`,
`LORA_TX_PWR`) and timing constants
(`TX_CYCLE_INTERVAL_MS`, `SENSOR_POLL_INTERVAL_MS`, `HEARTBEAT_INTERVAL_MS`)
also live in `Config.hpp`.

## Source layout

```
include/New/
  Config.hpp            feature flags, pin map, constants, globals
  Sensors.hpp           reading structs + Sensors class
  MavlinkFrames.hpp     one builder per sensor type
  LoraTx.hpp            radio wrapper + retry ring buffer
  Storage.hpp           SD + LittleFS CSV mirror
  App.hpp               orchestrator (setup-time + per-cycle work)

src/New/
  main.cpp              shared globals, geiger ISRs, setup()/loop()
  Sensors.cpp           ICM-45686 / BMP388 / BMM350 / TMP1075 / DAC43401
  MavlinkFrames.cpp     MAVLink wire-format packers
  LoraTx.cpp            SX1280 init + BUSY-polled TX + retry queue
  Storage.cpp           CSV append + flash dump / wipe
  App.cpp               per-cycle TX flow + serial commands
```

## Flash partition note

`platformio.ini` uses `board_build.partitions = default.csv`, which is the
4 MB layout. On the 16 MB chip that means LittleFS only gets ~1.4 MB. The
quota in `Config.hpp` is set to 1.5 MB so it almost matches; if you want
the full ~3.4 MB filesystem partition, switch the partitions line to
`default_16MB.csv` and bump `FLASH_QUOTA_B` to `3 * 1024 * 1024`.
