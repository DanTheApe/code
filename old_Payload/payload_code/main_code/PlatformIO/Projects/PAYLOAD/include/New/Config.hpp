#pragma once

// =============================================================================
//  Config.hpp
//  Central configuration for the "New" payload firmware.
//  Contains feature switches, pin map, I2C addresses, LoRa radio settings,
//  MAVLink identity, timing constants, and shared globals.
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "esp_system.h"

// -----------------------------------------------------------------------------
//  Feature switches (1 = enabled, 0 = disabled).
//  Disable any peripheral that is missing on the bench so the firmware still
//  boots and continues to log whatever sensors are present.
// -----------------------------------------------------------------------------
#define USE_SERIAL     1   // Print status/debug to USB CDC.
#define PRINT_TX       0   // Print every MAVLink frame as it goes out the radio.
#define PRINT_TX_HEX   0   // Also dump raw bytes (hex) of every transmitted frame.
#define USE_LORA       0   // SX1280 transmitter + MAVLink frames.
#define USE_SD         1   // microSD card on PIN_SD_CS.
#define USE_FLASH      1   // LittleFS log mirror in onboard flash.
#define USE_GEIGER     1   // Geiger pulse counter (CRD1 + CRD2).
#define USE_IMU        0   // ICM-45686 accelerometer + gyroscope.
#define USE_BARO       0   // BMP388 barometer.
#define USE_MAG        0   // BMM350 magnetometer.
#define USE_LORA_TEMP  0   // TMP1075 temperature sensor next to LoRa module.
#define USE_LORA_VOLT  0   // DAC43401 voltage setting that drives LoRa TX power.

// -----------------------------------------------------------------------------
//  Conditional library includes.
// -----------------------------------------------------------------------------
#if USE_SD || USE_FLASH
  #include <FS.h>
#endif
#if USE_SD
  #include <SD.h>
#endif
#if USE_FLASH
  #include <LittleFS.h>
#endif
#if USE_LORA
  #include <RadioLib.h>
  #include <mavlink/v2.0/common/mavlink.h>
#endif
#if USE_IMU
  #include <ICM45686.h>
#endif
#if USE_BARO
  #include <Adafruit_BMP3XX.h>
#endif
#if USE_MAG
  #include "DFRobot_BMM350.h"
#endif

// -----------------------------------------------------------------------------
//  Pin map — values come straight from the breakout pin table in
//  "Main board testing.pdf". Do not change without updating the hardware doc.
// -----------------------------------------------------------------------------
constexpr int PIN_I2C_SDA   = 7;    // I2C data line for all sensors.
constexpr int PIN_I2C_SCL   = 15;   // I2C clock line for all sensors.

constexpr int PIN_SPI_MOSI  = 4;    // Shared SPI MOSI (SD + LoRa + IMU).
constexpr int PIN_SPI_SCK   = 5;    // Shared SPI clock.
constexpr int PIN_SPI_MISO  = 6;    // Shared SPI MISO.

constexpr int PIN_IMU_CS    = 16;   // ICM-45686 chip-select  (IF8).
constexpr int PIN_IMU_INT1  = 17;   // ICM-45686 INT1         (IF9).
constexpr int PIN_IMU_INT2  = 18;   // ICM-45686 INT2         (IF10).

constexpr int PIN_GEIGER_1  = 8;    // Geiger CRD1 pulse input (IF11).
constexpr int PIN_GEIGER_2  = 3;    // Geiger CRD2 pulse input (IF12).

constexpr int PIN_LORA_CS    = 9;   // SX1280 chip-select       (IF13).
constexpr int PIN_LORA_PWR   = 10;  // SX1280 power-enable      (IF14).
constexpr int PIN_LORA_DIO1  = 11;  // SX1280 DIO1 / CXT        (IF15).
constexpr int PIN_LORA_DIO2  = 12;  // SX1280 DIO2 / CrX        (IF16).
constexpr int PIN_LORA_BUSY  = 13;  // SX1280 BUSY              (IF17).
constexpr int PIN_LORA_RST   = 14;  // SX1280 RESET             (IF18).

constexpr int PIN_SD_CS      = 21;  // microSD chip-select.

// -----------------------------------------------------------------------------
//  I2C device addresses.
// -----------------------------------------------------------------------------
constexpr uint8_t I2C_ADDR_BMM350   = 0x14;  // Magnetometer.
constexpr uint8_t I2C_ADDR_BMP388   = 0x76;  // Barometer (SDO low).
// NOTE: the breakout PDF says DAC=0x72 / TMP=0x73, but the actual board
// strapping puts these chips on the TI default range.  test_lora_temp
// confirmed TMP1075 at 0x49; DAC43401 has been observed at 0x48 (or 0x47
// when the ADDR pin is tied differently).
constexpr uint8_t I2C_ADDR_DAC43401 = 0x48;  // Sets LoRa TX power voltage.
constexpr uint8_t I2C_ADDR_TMP1075  = 0x49;  // Monitors LoRa module temperature.

// -----------------------------------------------------------------------------
//  LoRa radio (SX1280, 2.4 GHz).
// -----------------------------------------------------------------------------
constexpr float   LORA_FREQ_MHZ = 2400.0f;   // Carrier frequency in MHz.
constexpr float   LORA_BW_KHZ   = 812.5f;    // Bandwidth in kHz.
constexpr uint8_t LORA_SF       = 7;         // Spreading factor.
constexpr uint8_t LORA_CR       = 5;         // Coding rate denominator (5 = 4/5).
constexpr int8_t  LORA_TX_PWR   = 13;        // Output power in dBm (max +13 dBm).

// -----------------------------------------------------------------------------
//  MAVLink identity for outgoing frames.
// -----------------------------------------------------------------------------
constexpr uint8_t MAV_SYSID   = 42;          // Payload's MAVLink system ID.
constexpr uint8_t MAV_COMPID  = 191;         // Component ID for "scientific payload".

// -----------------------------------------------------------------------------
//  Timing.
// -----------------------------------------------------------------------------
constexpr uint32_t TX_CYCLE_INTERVAL_MS    = 1000;  // One full TX cycle per second.
constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 50;    // Refresh latest values 20 Hz.
constexpr uint32_t HEARTBEAT_INTERVAL_MS   = 5000;  // MAVLink heartbeat cadence.

// -----------------------------------------------------------------------------
//  LoRa retry queue. If a frame fails to transmit it is pushed onto the queue
//  and re-tried on the next service tick until LORA_MAX_ATTEMPTS is reached,
//  after which the frame is dropped (and counted in totalDropped()).
// -----------------------------------------------------------------------------
constexpr size_t   LORA_FRAME_MAX     = 64;  // Max bytes any one MAVLink frame may take.
constexpr size_t   LORA_RETRY_QUEUE   = 16;  // Number of frames the queue may hold.
constexpr uint8_t  LORA_MAX_ATTEMPTS  = 3;   // Drop after this many failed sends.
constexpr uint16_t LORA_BACKOFF_MS    = 30;  // Small delay between back-to-back sends.

// -----------------------------------------------------------------------------
//  Storage paths.
// -----------------------------------------------------------------------------
constexpr const char* FLASH_LOG_DIR = "/logs";
constexpr size_t      FLASH_QUOTA_B = static_cast<size_t>(1.5 * 1024 * 1024);
constexpr const char* SD_LOG_DIR    = "/logs";

// -----------------------------------------------------------------------------
//  CSV header for both SD and flash row logs.
// -----------------------------------------------------------------------------
extern const char* CSV_HEADER;

// -----------------------------------------------------------------------------
//  Globals shared across translation units. Defined in main.cpp.
// -----------------------------------------------------------------------------
extern Preferences prefs;                          // Boot-persistent key/value store.
extern uint32_t    runId;                          // Increments every cold boot.
extern volatile uint32_t geigerCh1Pulses;          // ISR counter for CRD1.
extern volatile uint32_t geigerCh2Pulses;          // ISR counter for CRD2.

// -----------------------------------------------------------------------------
//  Geiger ISRs — defined in main.cpp.
// -----------------------------------------------------------------------------
void IRAM_ATTR geigerCh1Isr();
void IRAM_ATTR geigerCh2Isr();
