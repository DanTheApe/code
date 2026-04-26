#include "New/Sensors.hpp"

// =============================================================================
//  Sensors.cpp
//  Implements every sensor read on the payload board. Drivers are pulled in
//  conditionally so this single TU compiles even when most sensors are
//  disabled in Config.hpp.
// =============================================================================

#if USE_IMU
static ICM456xx s_imu(SPI, PIN_IMU_CS);
#endif
#if USE_BARO
static Adafruit_BMP3XX s_bmp;
#endif
#if USE_MAG
static DFRobot_BMM350_I2C s_bmm(&Wire, I2C_ADDR_BMM350);
#endif

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

// Input  : v - any 32-bit signed value.
// What   : clips it into the int16_t range.
// Output : the clipped value.
static int16_t clamp16(int32_t v) {
  if (v >  32767) return  32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

// Input  : addr - I2C address of a slave to probe.
// What   : performs an empty I2C transaction to see if the slave ACKs.
// Output : true if the slave is present, false otherwise.
static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// IMU configured full-scale ranges (must match the values passed to startAccel
// / startGyro in begin()). Raw counts are int16_t; full-scale = ±FSR_*.
constexpr int32_t IMU_ACCEL_FSR_G   = 16;     // ±16 g.
constexpr int32_t IMU_GYRO_FSR_DPS  = 2000;   // ±2000 dps.

// Input  : raw - signed 16-bit accelerometer count from the ICM-45686.
// What   : converts raw count to milli-g using the configured FSR.
//          mg = raw * FSR_g * 1000 / 32768.
// Output : 16-bit clipped milli-g value.
static int16_t imuRawToMg(int16_t raw) {
  return clamp16(static_cast<int32_t>(raw) * IMU_ACCEL_FSR_G * 1000 / 32768);
}

// Input  : raw - signed 16-bit gyro count from the ICM-45686.
// What   : converts raw count to milli-rad/s using the configured FSR.
//          mrad/s = raw * FSR_dps * 1000 * pi/180 / 32768
//                ≈ raw * FSR_dps * 17453 / (32768 * 1000).
// Output : 16-bit clipped milli-rad/s value.
static int16_t imuRawToMrad(int16_t raw) {
  // For ±2000 dps full-scale this saturates int16 at about ±1878 dps which
  // is far above any reasonable rotation rate for a free-falling payload.
  return clamp16(static_cast<int32_t>(raw) * IMU_GYRO_FSR_DPS * 17453
                 / (32768L * 1000L));
}

// =============================================================================
//  Sensors::begin
// =============================================================================
bool Sensors::begin() {
  bool any = false;

#if USE_IMU
  imuOk_ = (s_imu.begin() == 0);
  if (imuOk_) {
    s_imu.startAccel(100, 16);    // 100 Hz, ±16 g.
    s_imu.startGyro(100, 2000);   // 100 Hz, ±2000 dps.
#if USE_SERIAL
    Serial.println("[SENS] ICM-45686 OK");
#endif
    any = true;
  } else {
#if USE_SERIAL
    Serial.println("[SENS] ICM-45686 init failed");
#endif
  }
#endif

#if USE_BARO
  baroOk_ = s_bmp.begin_I2C(I2C_ADDR_BMP388, &Wire);
  if (baroOk_) {
    s_bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    s_bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    s_bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
#if USE_SERIAL
    Serial.println("[SENS] BMP388 OK");
#endif
    any = true;
  } else {
#if USE_SERIAL
    Serial.println("[SENS] BMP388 init failed");
#endif
  }
#endif

#if USE_MAG
  magOk_ = (s_bmm.begin() == 0);
  if (magOk_) {
    // BMM350 powers up in suspend mode. We must explicitly switch it to
    // normal-mode and let the first conversion finish, otherwise the next
    // getGeomagneticData() call returns uninitialized garbage.
    s_bmm.setOperationMode(eBmm350NormalMode);
    delay(100);
  }
#if USE_SERIAL
  Serial.println(magOk_ ? "[SENS] BMM350 OK" : "[SENS] BMM350 init failed");
#endif
  if (magOk_) any = true;
#endif

#if USE_LORA_TEMP
  tmp1075Ok_ = i2cPresent(I2C_ADDR_TMP1075);
#if USE_SERIAL
  Serial.println(tmp1075Ok_ ? "[SENS] TMP1075 present" : "[SENS] TMP1075 missing");
#endif
  if (tmp1075Ok_) any = true;
#endif

#if USE_LORA_VOLT
  dac43401Ok_ = i2cPresent(I2C_ADDR_DAC43401);
#if USE_SERIAL
  Serial.println(dac43401Ok_ ? "[SENS] DAC43401 present" : "[SENS] DAC43401 missing");
#endif
  if (dac43401Ok_) any = true;
#endif

  return any;
}

// =============================================================================
//  Sensors::poll
// =============================================================================
void Sensors::poll(uint32_t nowMs) {
#if USE_IMU
  if (imuOk_) {
    inv_imu_sensor_data_t d;
    if (s_imu.getDataFromRegisters(d) == 0) {
      // accel_data / gyro_data are RAW int16 counts at the configured FSR
      // (NOT g / dps). Convert through imuRawToMg / imuRawToMrad.
      accel_.ok    = true;
      accel_.t_ms  = nowMs;
      accel_.x_mg  = imuRawToMg(d.accel_data[0]);
      accel_.y_mg  = imuRawToMg(d.accel_data[1]);
      accel_.z_mg  = imuRawToMg(d.accel_data[2]);

      gyro_.ok       = true;
      gyro_.t_ms     = nowMs;
      gyro_.x_mrad_s = imuRawToMrad(d.gyro_data[0]);
      gyro_.y_mrad_s = imuRawToMrad(d.gyro_data[1]);
      gyro_.z_mrad_s = imuRawToMrad(d.gyro_data[2]);
    } else {
      accel_.ok = false;
      gyro_.ok  = false;
    }
  }
#endif

#if USE_BARO
  if (baroOk_) {
    if (s_bmp.performReading()) {
      baro_.ok             = true;
      baro_.t_ms           = nowMs;
      baro_.pressure_hPa   = static_cast<float>(s_bmp.pressure) / 100.0f;
      baro_.temperature_cC = clamp16(static_cast<int32_t>(s_bmp.temperature * 100.0f));
    } else {
      baro_.ok = false;
    }
  }
#endif

#if USE_MAG
  if (magOk_) {
    sBmm350MagData_t m = s_bmm.getGeomagneticData();
    // m.x / m.y / m.z are scaled int32 internals — NOT µT.
    // The actual µT values are in m.float_x / m.float_y / m.float_z.
    // 1 µT = 10 mgauss, so multiply by 10 to match MAVLink scaled_imu units.
    mag_.ok       = true;
    mag_.t_ms     = nowMs;
    mag_.x_mgauss = clamp16(static_cast<int32_t>(m.float_x * 10.0f));
    mag_.y_mgauss = clamp16(static_cast<int32_t>(m.float_y * 10.0f));
    mag_.z_mgauss = clamp16(static_cast<int32_t>(m.float_z * 10.0f));
  }
#endif
}

// =============================================================================
//  Sensors::takeGeiger
// =============================================================================
GeigerReading Sensors::takeGeiger(uint32_t nowMs) {
  GeigerReading r;
  r.t_ms = nowMs;
#if USE_GEIGER
  noInterrupts();
  r.counts1 = geigerCh1Pulses;
  r.counts2 = geigerCh2Pulses;
  geigerCh1Pulses = 0;
  geigerCh2Pulses = 0;
  interrupts();
  r.ok = true;
#else
  r.ok = false;
#endif
  return r;
}

// =============================================================================
//  Sensors::readLoraTemp  -  TMP1075 over I2C
// =============================================================================
LoraTempReading Sensors::readLoraTemp(uint32_t nowMs) {
  LoraTempReading r;
  r.t_ms = nowMs;
#if USE_LORA_TEMP
  if (!tmp1075Ok_) return r;

  // Point at the temperature register (reg 0x00).
  Wire.beginTransmission(I2C_ADDR_TMP1075);
  Wire.write(static_cast<uint8_t>(0x00));
  if (Wire.endTransmission(false) != 0) return r;

  if (Wire.requestFrom(static_cast<int>(I2C_ADDR_TMP1075), 2) != 2) return r;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();

  // 12-bit signed value, MSB-aligned in the 16-bit register, 0.0625 °C/LSB.
  int16_t raw = static_cast<int16_t>((hi << 8) | lo);
  int16_t v12 = raw >> 4;                          // arithmetic shift keeps sign.
  // Centi-degrees C = v12 * 6.25 = v12 * 625 / 100.
  int32_t cC = (static_cast<int32_t>(v12) * 625) / 100;

  r.ok   = true;
  r.t_cC = clamp16(cC);
#endif
  return r;
}

// =============================================================================
//  Sensors::readLoraVolt  -  reports the cached DAC43401 setpoint
// =============================================================================
LoraVoltReading Sensors::readLoraVolt(uint32_t nowMs) {
  LoraVoltReading r;
  r.t_ms = nowMs;
#if USE_LORA_VOLT
  if (dac43401Ok_) {
    r.ok = true;
    r.mV = cachedDacMv_;
  }
#endif
  return r;
}

// =============================================================================
//  Sensors::setLoraVolt  -  writes a new setpoint to the DAC43401
// =============================================================================
bool Sensors::setLoraVolt(uint16_t mV) {
#if USE_LORA_VOLT
  if (!dac43401Ok_) return false;

  // DAC43401: 8-bit single-channel DAC, full scale = 3.3 V.
  // Output mV = code * 3300 / 255 -> code = mV * 255 / 3300.
  if (mV > 3300) mV = 3300;
  uint8_t code = static_cast<uint8_t>((static_cast<uint32_t>(mV) * 255u) / 3300u);

  // Write to DAC_DATA register (0x21) per TI datasheet, MSB-aligned 8-bit code.
  Wire.beginTransmission(I2C_ADDR_DAC43401);
  Wire.write(static_cast<uint8_t>(0x21));
  Wire.write(code);
  Wire.write(static_cast<uint8_t>(0x00));
  if (Wire.endTransmission() != 0) return false;

  cachedDacMv_ = mV;
  return true;
#else
  (void)mV;
  return false;
#endif
}
