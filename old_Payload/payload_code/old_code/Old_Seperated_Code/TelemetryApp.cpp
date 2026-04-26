#include "TelemetryApp.hpp"

#if Geiger
static GeigerCounter geiger(GEIGER_PIN);
#endif

#if IMU_Sensor
ICM456xx IMU(Wire, 0);
static bool imuOk = false;
#endif

#if Baro_Sensor
Adafruit_BMP3XX bmp;
static bool baroOk = false;
static const uint8_t BMP390_ADDR = 0x77;
#endif

#if Mag_Sensor
static const uint8_t BMM350_ADDR = 0x14;
DFRobot_BMM350_I2C bmm350(&Wire, BMM350_ADDR);
static bool magOk = false;
#endif

static void i2cScanPrint() {
  #if WrightToSerial
  Serial.println("[I2C] scanning...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  found 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  Serial.printf("[I2C] scan done, %u device(s)\n", found);
  #endif
}

static inline int16_t clamp16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

#if flashmemori
static void logBootReasonToFlash() {
  File f = LittleFS.open("/boot_reason.txt", FILE_WRITE);
  if (!f) return;
  f.printf("reset=%u (%s)\n",
           static_cast<unsigned>(g_boot_reason_raw),
           resetReasonStr(static_cast<esp_reset_reason_t>(g_boot_reason_raw)));
  f.close();
}
#endif

static SensorSnapshot readSensorsOnce() {
  SensorSnapshot s;

  #if IMU_Sensor
    if (imuOk) {
      inv_imu_sensor_data_t data;
      if (IMU.getDataFromRegisters(data) == 0) {
        s.ax_mg = clamp16(static_cast<int32_t>(data.accel_data[0] * 1000.0f));
        s.ay_mg = clamp16(static_cast<int32_t>(data.accel_data[1] * 1000.0f));
        s.az_mg = clamp16(static_cast<int32_t>(data.accel_data[2] * 1000.0f));
        s.gx_mdps = clamp16(static_cast<int32_t>(data.gyro_data[0] * 1000.0f));
        s.gy_mdps = clamp16(static_cast<int32_t>(data.gyro_data[1] * 1000.0f));
        s.gz_mdps = clamp16(static_cast<int32_t>(data.gyro_data[2] * 1000.0f));
        s.okMask |= (1 << 0);
      }
    }
  #endif

  #if Baro_Sensor
    if (baroOk) {
      if (bmp.performReading()) {
        s.t_cC = clamp16(static_cast<int32_t>(bmp.temperature * 100.0f));
        s.p_Pa = static_cast<int32_t>(bmp.pressure);
        s.okMask |= (1 << 1);
      }
    }
  #endif

  #if Mag_Sensor
    if (magOk) {
      sBmm350MagData_t m = bmm350.getGeomagneticData();
      s.mx_uT10 = clamp16(static_cast<int32_t>(m.x * 10.0f));
      s.my_uT10 = clamp16(static_cast<int32_t>(m.y * 10.0f));
      s.mz_uT10 = clamp16(static_cast<int32_t>(m.z * 10.0f));
      s.okMask |= (1 << 2);
    }
  #endif

  return s;
}

void TelemetryApp::begin() {
  #if WrightToSerial
    Serial.begin(115200);
    for (uint32_t t = millis(); millis() - t < 150;) {}
  #endif

  g_boot_reason_raw = static_cast<uint32_t>(esp_reset_reason());
  #if WrightToSerial
    Serial.printf("[BOOT] reset reason=%u (%s)\n",
                  static_cast<unsigned>(g_boot_reason_raw),
                  resetReasonStr(static_cast<esp_reset_reason_t>(g_boot_reason_raw)));
    Serial.println("[CMD] Type 'clear' (wipe flash), 'status', 'print', or 'new' (rotate SD/LFS files).");
  #endif

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  i2cScanPrint();

  prefs.begin("logger", false);
  runId = prefs.getUInt("run", 0) + 1;
  prefs.putUInt("run", runId);

  String savedPath = prefs.getString("sd_path", "/logs/telemetry.csv");
  prefs.end();

  #if SDKort
    sdOk_ = sd_.begin();
    sd_.setDesiredPath(savedPath);
    sdDesiredPath_ = savedPath;
  #endif

  #if flashmemori
    flashOk_ = flash_.begin();
    if (flashOk_) {
      logBootReasonToFlash();
      flashPath_ = flash_.makeLogPath(runId);
      flash_.ensureHeader(flashPath_);
      #if WrightToSerial
        Serial.printf("[FLASH] LittleFS total=%u used=%u bytes\n",
                      static_cast<unsigned>(flash_.totalBytes()),
                      static_cast<unsigned>(flash_.usedBytes()));
        Serial.print("[FLASH] Logging to: ");
        Serial.println(flashPath_);
      #endif
    }
  #endif

  #if Geiger
    geiger.begin();
  #endif

  #if LoRa
    loraOk_ = lora_.begin();
    if (!loraOk_) Serial.println("[LoRa] init failed, will keep running, logging status codes.");
  #endif

  #if IMU_Sensor
    imuOk = (IMU.begin() == 0);
    if (imuOk) {
      IMU.startAccel(100, 16);
      IMU.startGyro(100, 2000);
      #if WrightToSerial
        Serial.println("[IMU] ICM456xx OK");
      #endif
    } else {
      #if WrightToSerial
        Serial.println("[IMU] ICM456xx init failed");
      #endif
    }
  #endif

  #if Baro_Sensor
    baroOk = bmp.begin_I2C(BMP390_ADDR, &Wire);
    if (baroOk) {
      bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
      bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
      bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
      #if WrightToSerial
        Serial.println("[BARO] BMP390 OK");
      #endif
    } else {
      #if WrightToSerial
        Serial.println("[BARO] BMP390 init failed (check addr 0x77/0x76)");
      #endif
    }
  #endif

  #if Mag_Sensor
    magOk = (bmm350.begin() == 0);
    #if WrightToSerial
      Serial.println(magOk ? "[MAG] BMM350 OK" : "[MAG] BMM350 init failed");
    #endif
  #endif

  lastTick_ = millis();
}

void TelemetryApp::tick() {
  processSerialCommands();

  static bool printedBootEcho = false;
  if (!printedBootEcho) {
    #if WrightToSerial
      Serial.printf("[BOOT] (echo) reset reason=%u (%s)\n",
                    static_cast<unsigned>(g_boot_reason_raw),
                    resetReasonStr(static_cast<esp_reset_reason_t>(g_boot_reason_raw)));
    #endif
    printedBootEcho = true;
  }

  uint32_t now = millis();

  #if SDKort
    if (!sdOk_) {
      sd_.maybeRemount(now);
      sdOk_ = sd_.mounted();
    }
  #endif

  if (static_cast<uint32_t>(now - lastTick_) < LOG_INTERVAL_MS) return;
  lastTick_ = now;

  int c6s = 0;
  #if Geiger
    c6s = geiger.takeCountsAndReset();
  #endif

  TelemetrySample samp{now, c6s};
  loraBuf_.push(samp);

  int loraStatus = -32768;
  uint8_t usedRetries = 0;

  lastSens_ = readSensorsOnce();
  #if LoRa
    if (loraOk_) {
      if (loraBuf_.size() >= 3) {
        String payload = buildLoraPayload3();

        auto r = lora_.transmitWithRetry(payload, loRaRetries_);
        loraStatus = r.status;
        usedRetries = r.retriesUsed;

        if (r.status == RADIOLIB_ERR_NONE) {
          loraBuf_.popN(3);
        }
      } else {
        loraStatus = 1;
        usedRetries = 0;
      }
    } else {
      loraStatus = -1000;
      usedRetries = 0;
    }
  #else
    loraStatus = 0;
    usedRetries = 0;
  #endif

  String rowBase = String(runId) + "," +
                   String(now) + "," +
                   String(c6s) + "," +
                   String(static_cast<long>(lastSens_.p_Pa)) + "," +
                   String(static_cast<int>(lastSens_.t_cC)) + "," +
                   String(static_cast<int>(lastSens_.ax_mg)) + "," +
                   String(static_cast<int>(lastSens_.ay_mg)) + "," +
                   String(static_cast<int>(lastSens_.az_mg)) + "," +
                   String(static_cast<int>(lastSens_.gx_mdps)) + "," +
                   String(static_cast<int>(lastSens_.gy_mdps)) + "," +
                   String(static_cast<int>(lastSens_.gz_mdps)) + "," +
                   String(static_cast<int>(lastSens_.mx_uT10)) + "," +
                   String(static_cast<int>(lastSens_.my_uT10)) + "," +
                   String(static_cast<int>(lastSens_.mz_uT10)) + "," +
                   String(static_cast<unsigned>(lastSens_.okMask)) + "," +
                   String(loraStatus) + "," +
                   String(usedRetries) + ",";

  bool storageOkThisRow = true;

  #if SDKort
    if (sdOk_) {
      String sdRow = rowBase + "1," +
                     String(static_cast<unsigned long>(loRaRetries_)) + "," +
                     String(static_cast<unsigned long>(storageFails_));
      if (!sd_.appendSticky(sdRow)) {
        storageFails_++;
        storageOkThisRow = false;
        sdOk_ = false;
        #if WrightToSerial
          Serial.println("[LOG] SD write failed, will retry mount.");
        #endif
      }
    } else {
      storageOkThisRow = false;
    }
  #endif

  #if flashmemori
    if (flashOk_) {
      bool ok = flash_.append(flashPath_, rowBase + "1," +
                String(static_cast<unsigned long>(loRaRetries_)) + "," +
                String(static_cast<unsigned long>(storageFails_)));
      if (!ok) {
        storageFails_++;
        storageOkThisRow = false;
        #if WrightToSerial
          Serial.println("[LOG] FLASH write failed or full");
        #endif
      }
    }
  #endif

  #if WrightToSerial
    Serial.printf("[TX] t=%lu ms, c6s=%d, status=%d, retries=%u, LoRaRetriesTot=%lu, StorageFailsTot=%lu, StorageOK=%d\n",
                  static_cast<unsigned long>(now), c6s, loraStatus, usedRetries,
                  static_cast<unsigned long>(loRaRetries_),
                  static_cast<unsigned long>(storageFails_),
                  storageOkThisRow ? 1 : 0);
  #endif
}

String TelemetryApp::buildLoraPayload3() {
  String payload;

  for (size_t i = 0; i < 3; ++i) {
    const TelemetrySample& s = loraBuf_.peek(i);
    payload += "t" + String(i) + "=" + String(s.t_ms);
    payload += ",c" + String(i) + "=" + String(s.c6s);
    if (i < 2) payload += ",";
  }

  payload += ",pPa=" + String(static_cast<long>(lastSens_.p_Pa));
  payload += ",tCc=" + String(static_cast<int>(lastSens_.t_cC));
  payload += ",ax=" + String(static_cast<int>(lastSens_.ax_mg));
  payload += ",ay=" + String(static_cast<int>(lastSens_.ay_mg));
  payload += ",az=" + String(static_cast<int>(lastSens_.az_mg));
  payload += ",gx=" + String(static_cast<int>(lastSens_.gx_mdps));
  payload += ",gy=" + String(static_cast<int>(lastSens_.gy_mdps));
  payload += ",gz=" + String(static_cast<int>(lastSens_.gz_mdps));
  payload += ",mx=" + String(static_cast<int>(lastSens_.mx_uT10));
  payload += ",my=" + String(static_cast<int>(lastSens_.my_uT10));
  payload += ",mz=" + String(static_cast<int>(lastSens_.mz_uT10));
  payload += ",sOk=" + String(static_cast<unsigned>(lastSens_.okMask));

  payload += ",lrTot=" + String(static_cast<unsigned long>(loRaRetries_));
  payload += ",sfTot=" + String(static_cast<unsigned long>(storageFails_));

  return payload;
}

void TelemetryApp::processSerialCommands() {
  #if WrightToSerial
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      String cmd = serialBuf_;
      serialBuf_.clear();
      cmd.trim();
      cmd.toLowerCase();

      if (cmd == "clear") {
        #if flashmemori
          if (flashOk_) {
            flash_.wipeAllLogs();
            flash_.ensureHeader(flashPath_);
            Serial.printf("[FLASH] done. used=%u / total=%u bytes (quota=%u)\n",
                          static_cast<unsigned>(flash_.usedBytes()),
                          static_cast<unsigned>(flash_.totalBytes()),
                          static_cast<unsigned>(flash_.maxBytes()));
          } else {
            Serial.println("[FLASH] not initialized, nothing to clear.");
          }
        #else
          Serial.println("[FLASH] disabled at compiletime.");
        #endif
      } else if (cmd == "status") {
        #if flashmemori
          if (flashOk_) {
            Serial.printf("[STATUS] LittleFS used=%u / total=%u bytes, quota=%u, full=%s\n",
                          static_cast<unsigned>(flash_.usedBytes()),
                          static_cast<unsigned>(flash_.totalBytes()),
                          static_cast<unsigned>(flash_.maxBytes()),
                          flash_.isFull() ? "YES" : "NO");
            Serial.print("[STATUS] Current flash log: ");
            Serial.println(flashPath_);
          } else {
            Serial.println("[STATUS] Flash not initialized.");
          }
        #else
          Serial.println("[STATUS] Flash disabled at compiletime.");
        #endif
        #if SDKort
          Serial.print("[STATUS] SD mounted: ");
          Serial.println(sdOk_ ? "YES" : "NO");
          Serial.print("[STATUS] SD file: ");
          Serial.println(sdDesiredPath_);
        #endif
      } else if (cmd == "print") {
        #if flashmemori
          if (flashOk_) flash_.printAllLogsToSerial();
          else Serial.println("[FLASH] not initialized, nothing to print.");
        #else
          Serial.println("[FLASH] disabled at compiletime.");
        #endif
      } else if (cmd == "new") {
        #if SDKort
          prefs.begin("logger", false);
          uint32_t idx = prefs.getUInt("sd_idx", 0) + 1;
          prefs.putUInt("sd_idx", idx);

          char nameBuf[48];
          snprintf(nameBuf, sizeof(nameBuf), "/logs/telemetry_%03lu.csv", static_cast<unsigned long>(idx));
          sdDesiredPath_ = String(nameBuf);
          prefs.putString("sd_path", sdDesiredPath_);
          prefs.end();

          sd_.setDesiredPath(sdDesiredPath_);
          Serial.print("[SD] Rotated file to: ");
          Serial.println(sdDesiredPath_);
        #else
          Serial.println("[SD] disabled at compiletime.");
        #endif

        #if flashmemori
          flashPath_ = flash_.makeLogPath(++runId);
          flash_.ensureHeader(flashPath_);
          Serial.print("[FLASH] Rotated file to: ");
          Serial.println(flashPath_);
        #endif
      } else if (cmd.length()) {
        Serial.print("[CMD] Unknown: ");
        Serial.println(cmd);
        Serial.println("[CMD] Try: clear | status | print | new");
      }
    } else {
      if (serialBuf_.length() < 128) serialBuf_ += ch;
    }
  }
  #endif
}
