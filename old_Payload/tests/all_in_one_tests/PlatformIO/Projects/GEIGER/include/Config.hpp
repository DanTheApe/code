#pragma once

// Feature switches: set to 1 to enable, 0 to disable.
#define LoRa           1   // Enable LoRa radio support and packet transmission.
#define SDKort         0   // Enable SD card logging support.
#define Geiger         0   // Enable the Geiger counter input and counting logic.
#define flashmemori    1   // Mirror logs to onboard flash using LittleFS.
#define WrightToSerial 1   // Print status and debug messages to the serial monitor.
// Sensor switches: set to 1 to enable, 0 to disable.
#define IMU_Sensor     0   // Enable the ICM-45686 accelerometer/gyroscope.
#define Baro_Sensor    0   // Enable the BMP388 barometer.
#define Mag_Sensor     0   // Enable the BMM350 magnetometer.

#include <Arduino.h>      // Core Arduino types, String, timing, and serial support.
#include <SPI.h>          // SPI bus support for SD card and other peripherals.
#include <Preferences.h>  // ESP32 non-volatile key/value storage.
#include "esp_system.h"  // ESP32 reset reason and system utilities.
#include <Wire.h>         // I2C bus support for sensors.

#if IMU_Sensor
  #include <ICM45686.h>  // IMU driver used when the accelerometer/gyro is enabled.
#endif

#if Baro_Sensor
  #include <Adafruit_BMP3XX.h>  // Adafruit barometer driver for the BMP388/BMP390.
#endif

#if Mag_Sensor
  #include "DFRobot_BMM350.h"  // DFRobot magnetometer driver for the BMM350.
#endif

#if SDKort || flashmemori
  #include <FS.h>  // Generic file system interface used by SD and LittleFS.
#endif
#if SDKort
  #include <SD.h>  // SD card access library.
#endif
#if LoRa
  #include <RadioLib.h>  // Radio driver library used for LoRa transport.
  #include "common/mavlink.h"  // MAVLink common dialect (heartbeat, scaled_imu, etc.).

  // COSMIC_RADIATION+ identity in MAVLink frames.
  constexpr uint8_t MAVLINK_SYSID_COSMIC  = 42;   // System ID: COSMIC_RADIATION+
  constexpr uint8_t MAVLINK_COMPID_COSMIC = 191;  // Component ID: scientific payload

  // Binary MAVLink frame buffer: HEARTBEAT(21) + NV_INT(30) + SCALED_IMU(36) + SCALED_PRESSURE(28) = 115 bytes
  constexpr size_t MAVLINK_FRAME_BUF_SIZE = 150;
#endif
#if flashmemori
  #include <LittleFS.h>  // Onboard flash file system used for local log copies.
#endif

constexpr int PIN_I2C_SDA = 7;   // I2C SDA pin for the attached sensors.
constexpr int PIN_I2C_SCL = 15;  // I2C SCL pin for the attached sensors.

constexpr int PIN_SPI_MOSI = 4;  // SPI MOSI pin for SD or radio peripherals.
constexpr int PIN_SPI_SCK  = 5;  // SPI clock pin for SD or radio peripherals.
constexpr int PIN_SPI_MISO = 6;  // SPI MISO pin for SD or radio peripherals.

#if IMU_Sensor
constexpr int PIN_IMU_CS   = 16;  // ICM-45686 chip-select.  (IF8)
constexpr int PIN_IMU_INT1 = 17;  // ICM-45686 interrupt 1.  (IF9)
constexpr int PIN_IMU_INT2 = 18;  // ICM-45686 interrupt 2.  (IF10)
#endif

#if Geiger
constexpr int GEIGER_PIN = 8;  // Geiger pulse input, CRD1. (IF11)
constexpr int PIN_CRD2   = 3;  // Geiger channel 2, CRD2.   (IF12)
#endif

#if LoRa
constexpr int LORA_POWER_EN = 10;  // LoRa power enable.         (IF14)
constexpr int LORA_NSS      = 9;   // LoRa chip-select / NSS.    (IF13)
constexpr int LORA_DIO1     = 11;  // LoRa DIO1 / CXT interrupt. (IF15)
constexpr int LORA_DIO2     = 12;  // LoRa DIO2 / CrX.           (IF16)
constexpr int LORA_BUSY     = 13;  // LoRa busy pin.             (IF17)
constexpr int LORA_RST      = 14;  // LoRa reset pin.            (IF18)
#endif

#if SDKort
constexpr int SD_CS = 21;  // SD card chip-select pin.
#endif

// I2C device addresses.
constexpr uint8_t I2C_ADDR_BMM350   = 0x14;  // BMM350 magnetometer.
constexpr uint8_t I2C_ADDR_BMP388   = 0x76;  // BMP388 barometer (SDO low).
constexpr uint8_t I2C_ADDR_DAC43401 = 0x72;  // DAC43401 – sets LoRa TX power.
constexpr uint8_t I2C_ADDR_TMP1075  = 0x73;  // TMP1075  – monitors LoRa temp.

#if LoRa
constexpr float   LORA_FREQ_MHZ    = 2400.0;  // LoRa carrier frequency in MHz.
constexpr float   LORA_BW_KHZ      = 812.5;   // LoRa bandwidth in kHz.
constexpr uint8_t LORA_SF          = 7;       // LoRa spreading factor.
constexpr uint8_t LORA_CR          = 5;       // LoRa coding rate denominator (5 = 4/5).
constexpr int8_t  LORA_TX_PWR      = 13;      // LoRa transmit power in dBm. (SX1280 max is +13 dBm)
constexpr uint8_t MAX_LORA_RETRIES = 3;       // Maximum retry count for failed LoRa sends.
constexpr uint16_t BASE_BACKOFF_MS = 50;      // Base delay before retrying a failed LoRa send.
#endif

// Geiger is prioritized: send/save counts every 1 second.
constexpr uint32_t GEIGER_INTERVAL_MS = 1000;  // Geiger sampling and transmit interval in milliseconds.
// Poll other sensors frequently and keep latest values for the 1s packet.
constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 50;  // Fast sensor polling interval in milliseconds.

// Logical measurement IDs for LoRa payload records: ID|time_ms|value.
constexpr uint16_t MEAS_ID_GEIGER_C1S     = 101;    // Geiger counts for the current 1-second interval.
constexpr uint16_t MEAS_ID_BARO_PA        = 201;  // Barometer pressure in pascals.
constexpr uint16_t MEAS_ID_BARO_TC_CENTI  = 202;  // Barometer temperature in centi-degrees C.
constexpr uint16_t MEAS_ID_IMU_AX_MG      = 211;  // IMU X-axis acceleration in milli-g.
constexpr uint16_t MEAS_ID_IMU_AY_MG      = 212;  // IMU Y-axis acceleration in milli-g.
constexpr uint16_t MEAS_ID_IMU_AZ_MG      = 213;  // IMU Z-axis acceleration in milli-g.
constexpr uint16_t MEAS_ID_IMU_GX_MDPS    = 224;  // IMU X-axis gyro rate in milli-degrees/second.
constexpr uint16_t MEAS_ID_IMU_GY_MDPS    = 225;  // IMU Y-axis gyro rate in milli-degrees/second.
constexpr uint16_t MEAS_ID_IMU_GZ_MDPS    = 226;  // IMU Z-axis gyro rate in milli-degrees/second.
constexpr uint16_t MEAS_ID_MAG_MX_UT10    = 301;  // Magnetometer X-axis field in 0.1 uT.
constexpr uint16_t MEAS_ID_MAG_MY_UT10    = 302;  // Magnetometer Y-axis field in 0.1 uT.
constexpr uint16_t MEAS_ID_MAG_MZ_UT10    = 303;  // Magnetometer Z-axis field in 0.1 uT.
constexpr uint16_t MEAS_ID_SENSOR_OK_MASK = 401;  // Bitmask showing which sensors were valid.
constexpr uint16_t MEAS_ID_LORA_RETRY_TOT = 801;  // Total LoRa retries since boot.
constexpr uint16_t MEAS_ID_STORAGE_FAIL_TOT = 901;  // Total SD/flash write failures since boot.

// Keep radio payload conservative to fit LoRa and leave headroom for MAVLink framing.
constexpr size_t LORA_PACKET_MAX_BYTES = 180;  // Maximum text payload size before LoRa transmit.
constexpr size_t LORA_MAX_RECORDS_PER_PACKET = 10;  // Maximum buffered telemetry packets merged into one LoRa TX.

#if flashmemori
constexpr const char* FLASH_LOG_DIR = "/flogs";  // Directory used for LittleFS log files.
constexpr size_t FLASH_MAX_BYTES = static_cast<size_t>(1.5 * 1024 * 1024);  // Flash log quota in bytes.
#endif

struct SensorSnapshot {
  uint8_t okMask = 0;  // Status bitmask: bit0=IMU ok, bit1=BARO ok, bit2=MAG ok.

  int16_t ax_mg = 0, ay_mg = 0, az_mg = 0;  // IMU acceleration in milli-g.
  int16_t gx_mdps = 0, gy_mdps = 0, gz_mdps = 0;  // IMU gyro rate in milli-degrees/second.

  int32_t p_Pa = 0;   // Barometric pressure in pascals.
  int16_t t_cC = 0;   // Barometer temperature in centi-degrees C.

  int16_t mx_uT10 = 0, my_uT10 = 0, mz_uT10 = 0;  // Magnetometer field in 0.1 microtesla.
};

extern Preferences prefs;  // Shared NVS preferences handle.
extern uint32_t runId;  // Incrementing run identifier stored across boots.
extern RTC_DATA_ATTR uint32_t g_boot_reason_raw;  // Raw ESP32 reset reason preserved in RTC memory.
extern volatile uint32_t GEIGER_PULSE_COUNT;  // Geiger pulse counter updated by ISR.

extern const char* CSV_HEADER;  // CSV header used for saved logs.

const char* resetReasonStr(esp_reset_reason_t r);  // Convert ESP32 reset reason to text.
uint32_t readRtcResetReasonRaw();  // Read raw ROM reset reason code from CPU0.
const char* rtcResetReasonStr(uint32_t raw);  // Convert raw ROM reset reason code to text.
uint16_t rand16();  // Return a 16-bit random value.
void IRAM_ATTR geigerISR();  // Interrupt handler that counts geiger pulses.
