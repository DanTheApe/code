#pragma once

// Feature switches set to 1 to enable, 0 to disable.
#define LoRa           0   // set to 1 when LoRa is connected
#define SDKort         1   // set to 1 when SD card is connected
#define Geiger         0   // set to 1 when Geiger is connected
#define flashmemori    1   // set to 1 to mirror logs to onboard flash (LittleFS)
#define WrightToSerial 1   // set to 1 to write to serial monitor
// Trig
#define IMU_Sensor     0   // ICM-45686 accel/gyro
#define Baro_Sensor    0   // BMP390 pressure/temp
#define Mag_Sensor     0   // BMM350 magnetometer

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include "esp_system.h"
#include <Wire.h>

#if IMU_Sensor
  #include <ICM45686.h>
#endif

#if Baro_Sensor
  #include <Adafruit_BMP3XX.h>
#endif

#if Mag_Sensor
  #include "DFRobot_BMM350.h"
#endif

#if SDKort || flashmemori
  #include <FS.h>
#endif
#if SDKort
  #include <SD.h>
#endif
#if LoRa
  #include <RadioLib.h>
#endif
#if flashmemori
  #include <LittleFS.h>
#endif

constexpr int PIN_I2C_SDA = 7;
constexpr int PIN_I2C_SCL = 15;

constexpr int PIN_SPI_MOSI = 4;
constexpr int PIN_SPI_SCK  = 5;
constexpr int PIN_SPI_MISO = 6;

#if Geiger
constexpr int GEIGER_PIN = 17;
#endif

#if LoRa
constexpr int LORA_NSS   = 16;
constexpr int LORA_DIO1  = 26;
constexpr int LORA_RST   = 27;
constexpr int LORA_BUSY  = 25;
#endif

#if SDKort
constexpr int SD_CS = 21;
#endif

#if LoRa
constexpr float   LORA_FREQ_MHZ    = 2400.0;
constexpr float   LORA_BW_KHZ      = 812.5;
constexpr uint8_t LORA_SF          = 7;
constexpr uint8_t LORA_CR          = 5;
constexpr int8_t  LORA_TX_PWR      = 14;
constexpr uint8_t MAX_LORA_RETRIES = 3;
constexpr uint16_t BASE_BACKOFF_MS = 50;
#endif

// Geiger is prioritized: send/save counts every 1 second.
constexpr uint32_t GEIGER_INTERVAL_MS = 1000;
// Poll other sensors frequently and keep latest values for the 1s packet.
constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 50;

// Logical measurement IDs for LoRa payload records: ID|time_ms|value
constexpr uint16_t MEAS_ID_GEIGER_C1S     = 1;
constexpr uint16_t MEAS_ID_BARO_PA        = 101;
constexpr uint16_t MEAS_ID_BARO_TC_CENTI  = 102;
constexpr uint16_t MEAS_ID_IMU_AX_MG      = 201;
constexpr uint16_t MEAS_ID_IMU_AY_MG      = 202;
constexpr uint16_t MEAS_ID_IMU_AZ_MG      = 203;
constexpr uint16_t MEAS_ID_IMU_GX_MDPS    = 204;
constexpr uint16_t MEAS_ID_IMU_GY_MDPS    = 205;
constexpr uint16_t MEAS_ID_IMU_GZ_MDPS    = 206;
constexpr uint16_t MEAS_ID_MAG_MX_UT10    = 301;
constexpr uint16_t MEAS_ID_MAG_MY_UT10    = 302;
constexpr uint16_t MEAS_ID_MAG_MZ_UT10    = 303;
constexpr uint16_t MEAS_ID_SENSOR_OK_MASK = 401;
constexpr uint16_t MEAS_ID_LORA_RETRY_TOT = 901;
constexpr uint16_t MEAS_ID_STORAGE_FAIL_TOT = 902;

// Keep radio payload conservative to fit LoRa and leave headroom for MAVLink framing.
constexpr size_t LORA_PACKET_MAX_BYTES = 180;
constexpr size_t LORA_MAX_RECORDS_PER_PACKET = 10;

#if flashmemori
constexpr const char* FLASH_LOG_DIR = "/flogs";
constexpr size_t FLASH_MAX_BYTES = static_cast<size_t>(1.5 * 1024 * 1024);
#endif

struct SensorSnapshot {
  // status bitmask: bit0=IMU ok, bit1=BARO ok, bit2=MAG ok
  uint8_t okMask = 0;

  // IMU (scaled)
  int16_t ax_mg = 0, ay_mg = 0, az_mg = 0;
  int16_t gx_mdps = 0, gy_mdps = 0, gz_mdps = 0;

  // BARO
  int32_t p_Pa = 0;
  int16_t t_cC = 0;

  // MAG
  int16_t mx_uT10 = 0, my_uT10 = 0, mz_uT10 = 0;
};

extern Preferences prefs;
extern uint32_t runId;
extern RTC_DATA_ATTR uint32_t g_boot_reason_raw;
extern volatile uint32_t GEIGER_PULSE_COUNT;

extern const char* CSV_HEADER;

const char* resetReasonStr(esp_reset_reason_t r);
uint16_t rand16();
void IRAM_ATTR geigerISR();
