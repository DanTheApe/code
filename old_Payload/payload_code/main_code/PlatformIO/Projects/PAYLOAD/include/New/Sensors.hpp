#pragma once

// =============================================================================
//  Sensors.hpp
//  Encapsulates every sensor on the payload. Each public reading struct is
//  shaped so that its fields map 1:1 onto the MAVLink message types used to
//  transmit them (see MavlinkFrames.hpp).
// =============================================================================

#include "Config.hpp"

// -----------------------------------------------------------------------------
//  Reading structs. `ok` flag indicates whether the values are fresh.
//  Field types match MAVLink wire types so no extra casting is needed later.
// -----------------------------------------------------------------------------
struct AccelReading {
  bool     ok       = false;     // True when the latest read succeeded.
  uint32_t t_ms     = 0;         // Time of read (millis()).
  int16_t  x_mg     = 0;         // X acceleration in milli-g  (MAVLink xacc).
  int16_t  y_mg     = 0;         // Y acceleration in milli-g  (MAVLink yacc).
  int16_t  z_mg     = 0;         // Z acceleration in milli-g  (MAVLink zacc).
};

struct GyroReading {
  bool     ok       = false;
  uint32_t t_ms     = 0;
  int16_t  x_mrad_s = 0;         // X angular rate in milli-rad/s (MAVLink xgyro).
  int16_t  y_mrad_s = 0;         // Y angular rate in milli-rad/s.
  int16_t  z_mrad_s = 0;         // Z angular rate in milli-rad/s.
};

struct MagReading {
  bool     ok        = false;
  uint32_t t_ms      = 0;
  int16_t  x_mgauss  = 0;        // X field in mgauss (1 µT = 10 mgauss).
  int16_t  y_mgauss  = 0;
  int16_t  z_mgauss  = 0;
};

struct BaroReading {
  bool     ok            = false;
  uint32_t t_ms          = 0;
  float    pressure_hPa  = 0.0f; // Absolute pressure in hectopascals.
  int16_t  temperature_cC = 0;   // Temperature in centi-degC (cdegC).
};

struct GeigerReading {
  bool     ok       = false;
  uint32_t t_ms     = 0;
  uint32_t counts1  = 0;         // CRD1 counts since last takeAndReset().
  uint32_t counts2  = 0;         // CRD2 counts since last takeAndReset().
};

struct LoraTempReading {
  bool     ok    = false;
  uint32_t t_ms  = 0;
  int16_t  t_cC  = 0;            // Temperature in centi-degC.
};

struct LoraVoltReading {
  bool     ok    = false;
  uint32_t t_ms  = 0;
  uint16_t mV    = 0;            // Output voltage of the DAC in millivolts.
};

// -----------------------------------------------------------------------------
//  Sensors — one wrapper that owns drivers, state, and "ok" flags.
// -----------------------------------------------------------------------------
class Sensors {
public:
  // Input  : (none)
  // What   : powers up I2C/SPI peripherals and initializes every enabled driver.
  // Output : true if at least one sensor came up successfully, false otherwise.
  bool begin();

  // Input  : nowMs - current millis() timestamp.
  // What   : refreshes the latest accel/gyro/mag/baro values into the cached
  //          readings (geiger and LoRa monitors are read on demand instead).
  // Output : (none)
  void poll(uint32_t nowMs);

  // Input  : (none)
  // What   : returns the most recent cached reading without touching hardware.
  // Output : the cached reading struct.
  AccelReading lastAccel() const { return accel_; }
  GyroReading  lastGyro()  const { return gyro_;  }
  MagReading   lastMag()   const { return mag_;   }
  BaroReading  lastBaro()  const { return baro_;  }

  // Input  : nowMs - current millis() timestamp.
  // What   : atomically reads the geiger ISR counters and clears them.
  // Output : a GeigerReading containing the counts since the last call.
  GeigerReading takeGeiger(uint32_t nowMs);

  // Input  : nowMs - current millis() timestamp.
  // What   : performs a one-shot I2C read of the TMP1075 next to the LoRa chip.
  // Output : LoraTempReading with the temperature, or ok=false if the read fails.
  LoraTempReading readLoraTemp(uint32_t nowMs);

  // Input  : nowMs - current millis() timestamp.
  // What   : reports the current DAC43401 setting that controls LoRa TX power.
  // Output : LoraVoltReading with the cached output voltage in millivolts.
  LoraVoltReading readLoraVolt(uint32_t nowMs);

  // Input  : mV - desired DAC output in millivolts (0 .. 3300).
  // What   : writes the DAC43401 register so the LoRa power-amplifier voltage
  //          changes; also updates the cached value used by readLoraVolt().
  // Output : true on a successful I2C write, false otherwise.
  bool setLoraVolt(uint16_t mV);

  // Quick getters used in CSV row formatting.
  bool imuOk()  const { return imuOk_;  }
  bool baroOk() const { return baroOk_; }
  bool magOk()  const { return magOk_;  }

private:
  AccelReading accel_{};
  GyroReading  gyro_{};
  MagReading   mag_{};
  BaroReading  baro_{};

  bool imuOk_      = false;   // ICM-45686 came up at begin().
  bool baroOk_     = false;   // BMP388 came up at begin().
  bool magOk_      = false;   // BMM350 came up at begin().
  bool tmp1075Ok_  = false;   // TMP1075 detected on the bus.
  bool dac43401Ok_ = false;   // DAC43401 detected on the bus.

  uint16_t cachedDacMv_ = 0;  // Last value written to the DAC.
};
