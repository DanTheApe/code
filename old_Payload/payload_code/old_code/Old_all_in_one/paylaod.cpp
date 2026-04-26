// Links for reference
// https://elkim.no/wp-content/uploads/2021/06/esp-wroom-32_datasheet_en-1223836.pdf
// https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
// https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
// https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/483/SX1280-81_Rev3.2_Mar2020.pdf
// SX1280 datasheet: https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/483/SX1280-81_Rev3.2_Mar2020.pdf
// ESP32-S3 board package, Arduino core by Espressif
// Innstal: "ICM45686", "Adafruit BMP3XX Library" all. form Library


// Feature switches set to 1 to enable, 0 to disable.
#define LoRa           1   // set to 1 when LoRa is connected                       IMPORTENT
#define SDKort         1   // set to 1 when SD card is connected                    IMPORTENT
#define Geiger         1   // set to 1 when Geiger is connected                     IMPORTENT
#define flashmemori    1   // set to 1 to mirror logs to onboard flash (LittleFS)   IMPORTENT
#define WrightToSerial 1   // set to 1 to wright to serial monitor                  IMPORTENT
// Trig
#define IMU_Sensor     1   // ICM-45686 accel/gyro
#define Baro_Sensor    1   // BMP390 pressure/temp
#define Mag_Sensor     1   // BMM350 magnetometer


constexpr int PIN_I2C_SDA = 7;   // (I2C) SDA GPIO7
constexpr int PIN_I2C_SCL = 15;  // (I2C) SCL GPIO15
// Trig STOP
// Includes
#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include "esp_system.h"        // esp_reset_reason(), esp_random()
// Trig
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
//Trig STOP


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


// Trig
struct SensorSnapshot {
  // status bitmask: bit0=IMU ok, bit1=BARO ok, bit2=MAG ok
  uint8_t okMask = 0;


  // IMU (scaled)
  int16_t ax_mg = 0, ay_mg = 0, az_mg = 0;      // milli-g
  int16_t gx_mdps = 0, gy_mdps = 0, gz_mdps = 0; // milli-deg/s


  // BARO
  int32_t p_Pa = 0;   // pressure in Pa
  int16_t t_cC = 0;   // temperature in centi-degC (°C * 100)


  // MAG
  int16_t mx_uT10 = 0, my_uT10 = 0, mz_uT10 = 0; // microtesla * 10
};
// Trig STOP


// Boot reason capture & helpers
RTC_DATA_ATTR uint32_t g_boot_reason_raw = 0;


static const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT (RTC IO)";
    case ESP_RST_SW:        return "SW (esp_restart)";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}


#if flashmemori
static void logBootReasonToFlash() {
  File f = LittleFS.open("/boot_reason.txt", FILE_WRITE);
  if (!f) return;
  f.printf("reset=%u (%s)\n",
           (unsigned)g_boot_reason_raw,
           resetReasonStr((esp_reset_reason_t)g_boot_reason_raw));
  f.close();
}
#endif


// Geiger ISR (global)
volatile uint32_t GEIGER_PULSE_COUNT = 0;
void IRAM_ATTR geigerISR() { GEIGER_PULSE_COUNT++; }


// PCB SPI pins
constexpr int PIN_SPI_MOSI = 4;
constexpr int PIN_SPI_SCK  = 5;
constexpr int PIN_SPI_MISO = 6;


#if Geiger
  const int GEIGER_PIN = 17;     // IF9
#endif


// LoRa SX1280 pins
#if LoRa
  const int LORA_NSS   = 16;     // IF8
  const int LORA_DIO1  = 26;
  const int LORA_RST   = 27;
  const int LORA_BUSY  = 25;
#endif


// SD card CS
#if SDKort
  const int SD_CS = 21;          // SD_SS(CS)=21
#endif


// LoRa PHY params 2.4 GHz SX1280
#if LoRa
  const float   LORA_FREQ_MHZ   = 2400.0;
  const float   LORA_BW_KHZ     = 812.5;
  const uint8_t LORA_SF         = 7;
  const uint8_t LORA_CR         = 5;
  const int8_t  LORA_TX_PWR     = 14;
  const uint8_t MAX_LORA_RETRIES = 3;
  const uint16_t BASE_BACKOFF_MS = 50;
#endif


static const uint32_t LOG_INTERVAL_MS = 6000;     // 6 seconds


#if flashmemori
  static const char* FLASH_LOG_DIR = "/flogs";
  static const size_t FLASH_MAX_BYTES = 1.5 * 1024 * 1024;  // ~1.5 MiB
#endif




static const char* CSV_HEADER =
  "runId,t_ms,c/6s,"
  "p_Pa,t_cC,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps,mx_uT10,my_uT10,mz_uT10,sOkMask,"
  "LoRaStatus,Retries,StorageOK,LoRaRetriesTotal,StorageFailsTotal";


Preferences prefs;
uint32_t runId = 0;


static inline uint16_t rand16() { return (uint16_t)(esp_random() & 0xFFFF); }


// Geiger helper
class GeigerCounter {
public:
  explicit GeigerCounter(int pin) : pin_(pin) {}
  void begin() {
    pinMode(pin_, INPUT);
    attachInterrupt(pin_, geigerISR, RISING);
  }
  int takeCountsAndReset() {
    noInterrupts();
    uint32_t p = GEIGER_PULSE_COUNT;
    GEIGER_PULSE_COUNT = 0;
    interrupts();
    return (int)p;
  }
private: int pin_;
};
#if Geiger
  GeigerCounter geiger(GEIGER_PIN);
#endif


// SD CSV logger (hotinsert tolerant)
#if SDKort
class SdLogger {
public:
  explicit SdLogger(int csPin) : cs_(csPin) {}


  bool begin() {
    mounted_ = SD.begin(cs_, SPI, 8000000);
    if (mounted_) {
      SD.mkdir("/logs");
      #if WrightToSerial
        Serial.println("[SD] mounted");
      #endif
    } else {
      #if WrightToSerial
        Serial.println("[SD] mount failed");
      #endif
    }
    return mounted_;
  }


  void maybeRemount(uint32_t nowMs) {
    if (mounted_) return;
    if (nowMs - lastAttemptMs_ < mountRetryMs_) return;
    lastAttemptMs_ = nowMs;
    begin();
    if (mounted_ && desiredPath_.length()) {
      ensureHeader(desiredPath_);
      #if WrightToSerial
        Serial.print("[SD] Logging to: "); Serial.println(desiredPath_);
      #endif
      readyForAppends_ = true;
    }
  }


  void setDesiredPath(const String& p) {
    desiredPath_ = p;
    if (mounted_) {
      SD.mkdir("/logs");
      ensureHeader(desiredPath_);
      readyForAppends_ = true;
      #if WrightToSerial
        Serial.print("[SD] Logging to: "); Serial.println(desiredPath_);
      #endif
    } else {
      readyForAppends_ = false;
    }
  }


  bool appendSticky(const String& line) {
    if (!mounted_ || !readyForAppends_ || desiredPath_.isEmpty()) return false;


    File f = SD.open(desiredPath_, FILE_APPEND);
    if (!f) {
      SD.mkdir("/logs");
      ensureHeader(desiredPath_);
      f = SD.open(desiredPath_, FILE_APPEND);
      if (!f) {
        mounted_ = false;
        readyForAppends_ = false;
        return false;
      }
    }
    size_t n = f.println(line);
    f.close();
    if (n == 0) {
      mounted_ = false;
      readyForAppends_ = false;
      return false;
    }
    return true;
  }


  void ensureHeader(const String& path) {
    File fr = SD.open(path, FILE_READ);
    bool needHeader = true;
    if (fr) { needHeader = (fr.size() == 0); fr.close(); }
    if (needHeader) {
      File fw = SD.open(path, FILE_APPEND);
      if (fw) { fw.println(CSV_HEADER); fw.close(); }
    }
  }


  bool mounted() const { return mounted_; }
  String currentPath() const { return desiredPath_; }


private:
  int cs_;
  bool mounted_ = false;
  bool readyForAppends_ = false;
  String desiredPath_;
  uint32_t lastAttemptMs_ = 0;
  const uint32_t mountRetryMs_ = 1000;
};
#else
class SdLogger {
public:
  explicit SdLogger(int) {}
  bool begin() { return false; }
  void maybeRemount(uint32_t) {}
  void setDesiredPath(const String&) {}
  bool appendSticky(const String&) { return false; }
  String currentPath() const { return String(); }
};
#endif


// Flash (LittleFS) CSV logger with quota
#if flashmemori
class FlashLogger {
public:
  FlashLogger(const char* dir, size_t maxBytes) : dir_(dir), maxBytes_(maxBytes) {}


  bool begin() {
    if (!LittleFS.begin(true)) {
      #if WrightToSerial
        Serial.println("[FLASH] LittleFS mount failed");
      #endif
      return false;
    }
    LittleFS.mkdir(dir_);
    return true;
  }


  void wipeAllLogs() {
    #if WrightToSerial
      Serial.println("[FLASH] Wiping logs (Serial command)...");
    #endif
    deleteRecursive(dir_);
  }


  void printAllLogsToSerial() {
    #if WrightToSerial
      Serial.println("[FLASH]\tBEGIN PRINT ALL LOGS\t");
      printRecursive(dir_);
      Serial.println("[FLASH]\tEND PRINT ALL LOGS\t");
    #endif
  }


  String makeLogPath(uint32_t run) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s/run_%06lu.csv", dir_, (unsigned long)run);
    return String(buf);
  }


  void ensureHeader(const String& path) {
    File f = LittleFS.open(path, FILE_READ);
    bool needHeader = true;
    if (f) { needHeader = (f.size() == 0); f.close(); }
    if (needHeader) {
      File w = LittleFS.open(path, FILE_APPEND);
      if (w) { w.println(CSV_HEADER); w.close(); }
    }
  }


  bool append(const String& path, const String& line) {
    if (isFull()) {
      if (!stopped_) {
        stopped_ = true;
        #if WrightToSerial
          Serial.println("[FLASH] quota reached, stop writing further.");
        #endif
      }
      return false;
    }
    File f = LittleFS.open(path, FILE_APPEND);
    if (!f) return false;
    size_t n = f.println(line);
    f.close();
    return (n > 0);
  }


  bool isFull() const { return LittleFS.usedBytes() >= maxBytes_; }
  size_t usedBytes()  const { return LittleFS.usedBytes(); }
  size_t totalBytes() const { return LittleFS.totalBytes(); }
  size_t maxBytes()   const { return maxBytes_; }


private:
  void deleteRecursive(const char* path) {
    File root = LittleFS.open(path);
    if (!root) { LittleFS.mkdir(path); return; }
    if (root.isDirectory()) {
      File f = root.openNextFile();
      while (f) {
        String full;
        #if defined(ARDUINO_ARCH_ESP32)
          if (f.path()) full = f.path(); else
        #endif
        {
          String name = f.name();
          if (name.startsWith("/")) full = name;
          else                      full = String(path) + "/" + name;
        }
        if (f.isDirectory()) { f.close(); deleteRecursive(full.c_str()); }
        else { f.close(); LittleFS.remove(full.c_str()); }
        f = root.openNextFile();
      }
      root.close();
      LittleFS.rmdir(path);
      LittleFS.mkdir(path);
    } else {
      root.close();
      LittleFS.remove(path);
      LittleFS.mkdir(path);
    }
  }


  void printRecursive(const char* path) {
    File root = LittleFS.open(path);
    if (!root) {
      #if WrightToSerial
        Serial.print("[FLASH] Path not found: "); Serial.println(path);
      #endif
      return;
    }
    if (root.isDirectory()) {
      File f = root.openNextFile();
      while (f) {
        String full;
        #if defined(ARDUINO_ARCH_ESP32)
          if (f.path()) full = f.path(); else
        #endif
        {
          String name = f.name();
          if (name.startsWith("/")) full = name;
          else                      full = String(path) + "/" + name;
        }
        if (f.isDirectory()) { f.close(); printRecursive(full.c_str()); }
        else {
          size_t sz = f.size();
          #if WrightToSerial
            Serial.println();
            Serial.print("[FLASH]\tBEGIN FILE: "); Serial.print(full);
            Serial.print(" ("); Serial.print((unsigned)sz); Serial.println(" bytes)\t");
          #endif
          const size_t BUF_SZ = 512; uint8_t buf[BUF_SZ];
          while (f.available()) { size_t n = f.read(buf, BUF_SZ); if (n > 0) { Serial.write(buf, n); } else { break; } }
          f.close();
          #if WrightToSerial
            Serial.println();
            Serial.print("[FLASH]\tEND FILE: "); Serial.println(full);
          #endif
        }
        f = root.openNextFile();
      }
      root.close();
    } else {
      size_t sz = root.size();
      #if WrightToSerial
        Serial.println();
        Serial.print("[FLASH]\tBEGIN FILE: "); Serial.print(path);
        Serial.print(" ("); Serial.print((unsigned)sz); Serial.println(" bytes)\t");
      #endif
      const size_t BUF_SZ = 512; uint8_t buf[BUF_SZ];
      while (root.available()) { size_t n = root.read(buf, BUF_SZ); if (n > 0) { Serial.write(buf, n); } else { break; } }
      root.close();
      #if WrightToSerial
        Serial.println();
        Serial.print("[FLASH]\tEND FILE: "); Serial.println(path);
      #endif
    }
  }


  const char* dir_;
  size_t maxBytes_;
  bool stopped_ = false;
};
#else
class FlashLogger {
public:
  FlashLogger(const char*, size_t) {}
  bool begin() { return false; }
  void wipeAllLogs() {}
  void printAllLogsToSerial() {}
  String makeLogPath(uint32_t) { return String(); }
  void ensureHeader(const String&) {}
  bool append(const String&, const String&) { return false; }
  bool isFull() const { return true; }
  size_t usedBytes() const { return 0; }
  size_t totalBytes() const { return 0; }
  size_t maxBytes() const { return 0; }
};
#endif


// LoRa wrapper
#if LoRa
class LoRaLink {
public:
  LoRaLink(int nss, int dio1, int rst, int busy)
  : mod_(new Module(nss, dio1, rst, busy, SPI, SPISettings(8000000, MSBFIRST, SPI_MODE0)))
  , radio_(mod_) {}
  bool begin() {
    Serial.println("[LoRa] Begin");
    int state = radio_.begin();
    if (state != RADIOLIB_ERR_NONE) { Serial.printf("[LoRa] begin() failed: %d\n", state); return false; }
    if (radio_.setFrequency(LORA_FREQ_MHZ)   != RADIOLIB_ERR_NONE) return fail("setFrequency");
    if (radio_.setBandwidth(LORA_BW_KHZ)    != RADIOLIB_ERR_NONE) return fail("setBandwidth");
    if (radio_.setSpreadingFactor(LORA_SF)  != RADIOLIB_ERR_NONE) return fail("setSpreadingFactor");
    if (radio_.setCodingRate(LORA_CR)       != RADIOLIB_ERR_NONE) return fail("setCodingRate");
    if (radio_.setOutputPower(LORA_TX_PWR)  != RADIOLIB_ERR_NONE) return fail("setOutputPower");
    radio_.setCRC(true); radio_.setWhitening(true);
    Serial.println("[LoRa] Ready"); return true;
  }
  struct TxResult { int status; uint8_t retriesUsed; };
  TxResult transmitWithRetry(String& payload, uint32_t& retryAccumulator) {
    TxResult r { RADIOLIB_ERR_UNKNOWN, 0 };
    for (uint8_t attempt = 0; attempt <= MAX_LORA_RETRIES; ++attempt) {
      int s = radio_.transmit(payload);
      if (s == RADIOLIB_ERR_NONE) { r.status=s; r.retriesUsed=attempt; if (attempt>0) retryAccumulator+=attempt; return r; }
      if (attempt == MAX_LORA_RETRIES) { r.status=s; r.retriesUsed=attempt; retryAccumulator+=attempt; return r; }
      uint16_t jitter  = rand16() % 25;
      uint16_t backoff = (BASE_BACKOFF_MS << attempt) + jitter;
      delay(backoff);
    }
    return r;
  }
private:
  bool fail(const char* what) { Serial.print("[LoRa] "); Serial.print(what); Serial.println(" failed"); return false; }
  Module* mod_;
  SX1280  radio_;
};
#endif


// Ringbuffer types for LoRa telemetry
struct TelemetrySample {
  uint32_t t_ms;   // timestamp
  int      c6s;    // Geiger counts in 6 seconds window
};


class TelemetryRingBuffer {
public:
  static const size_t CAP = 10;


  void push(const TelemetrySample &s) {
    if (count_ == CAP) {
      tail_ = (tail_ + 1) % CAP;
      count_--;
    }
    buf_[head_] = s;
    head_ = (head_ + 1) % CAP;
    count_++;
  }


  size_t size() const { return count_; }


  const TelemetrySample &peek(size_t idxFromOldest) const {
    size_t idx = (tail_ + idxFromOldest) % CAP;
    return buf_[idx];
  }


  void popN(size_t n) {
    if (n > count_) n = count_;
    tail_ = (tail_ + n) % CAP;
    count_ -= n;
  }


private:
  TelemetrySample buf_[CAP];
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
};


// Trig
#if IMU_Sensor
  ICM456xx IMU(Wire, 0);  // lsb=0 => address typically 0x68; lsb=1 => 0x69. :contentReference[oaicite:8]{index=8}
  static bool imuOk = false;
#endif


#if Baro_Sensor
  Adafruit_BMP3XX bmp;
  static bool baroOk = false;
  static const uint8_t BMP390_ADDR = 0x77; // common default for BMP3xx boards :contentReference[oaicite:9]{index=9}
#endif


#if Mag_Sensor
  static const uint8_t BMM350_ADDR = 0x14; // DFRobot examples use 0x14 :contentReference[oaicite:10]{index=10}
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
  if (v >  32767) return  32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}


static SensorSnapshot readSensorsOnce() {
  SensorSnapshot s;


  #if IMU_Sensor
    if (imuOk) {
      // Library typically provides latest sample via internal buffer after startAccel/startGyro
      // Many examples use getDataFromRegisters(...) or similar, but the API guaranteed bits are
      // begin(), startAccel(), startGyro() per the library docs. :contentReference[oaicite:11]{index=11}
      inv_imu_sensor_data_t data;
      if (IMU.getDataFromRegisters(data) == 0) {
        // accel in g, gyro in dps (depends on library build); scale conservatively:
        s.ax_mg   = clamp16((int32_t)(data.accel_data[0] * 1000.0f));
        s.ay_mg   = clamp16((int32_t)(data.accel_data[1] * 1000.0f));
        s.az_mg   = clamp16((int32_t)(data.accel_data[2] * 1000.0f));
        s.gx_mdps = clamp16((int32_t)(data.gyro_data[0]  * 1000.0f));
        s.gy_mdps = clamp16((int32_t)(data.gyro_data[1]  * 1000.0f));
        s.gz_mdps = clamp16((int32_t)(data.gyro_data[2]  * 1000.0f));
        s.okMask |= (1 << 0);
      }
    }
  #endif


  #if Baro_Sensor
    if (baroOk) {
      if (bmp.performReading()) {
        s.t_cC = clamp16((int32_t)(bmp.temperature * 100.0f));
        // bmp.pressure is in Pa in Adafruit_BMP3XX
        s.p_Pa = (int32_t)(bmp.pressure);
        s.okMask |= (1 << 1);
      }
    }
  #endif


  #if Mag_Sensor
    if (magOk) {
      // DFRobot sample: sBmm350MagData_t magData = bmm350.getGeomagneticData(); :contentReference[oaicite:12]{index=12}
      sBmm350MagData_t m = bmm350.getGeomagneticData();
      s.mx_uT10 = clamp16((int32_t)(m.x * 10.0f));
      s.my_uT10 = clamp16((int32_t)(m.y * 10.0f));
      s.mz_uT10 = clamp16((int32_t)(m.z * 10.0f));
      s.okMask |= (1 << 2);
    }
  #endif


  return s;
}
// Trig STOP


// App orchestrator
class TelemetryApp {
public:
  void begin() {
    #if WrightToSerial
      Serial.begin(115200);
      for (uint32_t t = millis(); millis() - t < 150; ) { }
    #endif


    g_boot_reason_raw = (uint32_t)esp_reset_reason();
    #if WrightToSerial
      Serial.printf("[BOOT] reset reason=%u (%s)\n",
                    (unsigned)g_boot_reason_raw,
                    resetReasonStr((esp_reset_reason_t)g_boot_reason_raw));
      Serial.println("[CMD] Type 'clear' (wipe flash), 'status', 'print', or 'new' (rotate SD/LFS files).");
    #endif


    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);


    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);  // breakout I2C pins :contentReference[oaicite:14]{index=14}
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
                        (unsigned)flash_.totalBytes(), (unsigned)flash_.usedBytes());
          Serial.print("[FLASH] Logging to: "); Serial.println(flashPath_);
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


    //Trig
    #if IMU_Sensor
      imuOk = (IMU.begin() == 0);
      if (imuOk) {
        IMU.startAccel(100, 16); // ODR=100Hz, FSR=16G (library supported values) :contentReference[oaicite:15]{index=15}
        IMU.startGyro(100, 2000); // ODR=100Hz, FSR=2000 dps :contentReference[oaicite:16]{index=16}
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
      baroOk = bmp.begin_I2C(BMP390_ADDR, &Wire);  // default often 0x77 :contentReference[oaicite:17]{index=17}
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
    // Trig STOP


    lastTick_ = millis();
  }


  void tick() {
    processSerialCommands();


    static bool printedBootEcho = false;
    if (!printedBootEcho) {
      #if WrightToSerial
        Serial.printf("[BOOT] (echo) reset reason=%u (%s)\n",
                      (unsigned)g_boot_reason_raw,
                      resetReasonStr((esp_reset_reason_t)g_boot_reason_raw));
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


    if ((uint32_t)(now - lastTick_) < LOG_INTERVAL_MS) return;
    lastTick_ = now;


    int c6s = 0;
    #if Geiger
      c6s = geiger.takeCountsAndReset();
    #endif


    TelemetrySample samp { now, c6s };
    loraBuf_.push(samp);


    int loraStatus = -32768;
    uint8_t usedRetries = 0;


    lastSens_ = readSensorsOnce();
    #if LoRa
      if (loraOk_) {
        if (loraBuf_.size() >= 3) {
          String payload = buildLoraPayload3();


          auto r = lora_.transmitWithRetry(payload, loRaRetries_);
          loraStatus  = r.status;
          usedRetries = r.retriesUsed;


          if (r.status == RADIOLIB_ERR_NONE) {
            loraBuf_.popN(3);
          } else {
          }
        } else {
          loraStatus  = 1;
          usedRetries = 0;
        }
      } else {
        loraStatus  = -1000;
        usedRetries = 0;
      }
    #else
      loraStatus = 0; usedRetries = 0;
    #endif


    String rowBase = String(runId) + "," +
                     String(now) + "," +
                     String(c6s) + "," +
                     String((long)lastSens_.p_Pa) + "," +
                     String((int)lastSens_.t_cC) + "," +
                     String((int)lastSens_.ax_mg) + "," +
                     String((int)lastSens_.ay_mg) + "," +
                     String((int)lastSens_.az_mg) + "," +
                     String((int)lastSens_.gx_mdps) + "," +
                     String((int)lastSens_.gy_mdps) + "," +
                     String((int)lastSens_.gz_mdps) + "," +
                     String((int)lastSens_.mx_uT10) + "," +
                     String((int)lastSens_.my_uT10) + "," +
                     String((int)lastSens_.mz_uT10) + "," +
                     String((unsigned)lastSens_.okMask) + "," +
                     String(loraStatus) + "," +
                     String(usedRetries) + ",";


    bool storageOkThisRow = true;


    #if SDKort
      if (sdOk_) {
        String sdRow = rowBase + "1," +
                       String((unsigned long)loRaRetries_) + "," +
                       String((unsigned long)storageFails_);
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
               String((unsigned long)loRaRetries_) + "," +
               String((unsigned long)storageFails_));
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
                    (unsigned long)now, c6s, loraStatus, usedRetries,
                    (unsigned long)loRaRetries_, (unsigned long)storageFails_, storageOkThisRow ? 1 : 0);
    #endif
  }


private:
  String buildLoraPayload3() {
    String payload;


    for (size_t i = 0; i < 3; ++i) {
      const TelemetrySample &s = loraBuf_.peek(i);
      payload += "t" + String(i) + "=" + String(s.t_ms);
      payload += ",c" + String(i) + "=" + String(s.c6s);
      if (i < 2) payload += ",";
    }


    // latest sensor snapshot (scaled ints, compact)
    payload += ",pPa=" + String((long)lastSens_.p_Pa);
    payload += ",tCc=" + String((int)lastSens_.t_cC);
    payload += ",ax=" + String((int)lastSens_.ax_mg);
    payload += ",ay=" + String((int)lastSens_.ay_mg);
    payload += ",az=" + String((int)lastSens_.az_mg);
    payload += ",gx=" + String((int)lastSens_.gx_mdps);
    payload += ",gy=" + String((int)lastSens_.gy_mdps);
    payload += ",gz=" + String((int)lastSens_.gz_mdps);
    payload += ",mx=" + String((int)lastSens_.mx_uT10);
    payload += ",my=" + String((int)lastSens_.my_uT10);
    payload += ",mz=" + String((int)lastSens_.mz_uT10);
    payload += ",sOk=" + String((unsigned)lastSens_.okMask);


    payload += ",lrTot=" + String((unsigned long)loRaRetries_);
    payload += ",sfTot=" + String((unsigned long)storageFails_);


    return payload;
  }


  void processSerialCommands() {
    #if WrightToSerial
    while (Serial.available()) {
      char ch = (char)Serial.read();
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
                            (unsigned)flash_.usedBytes(),
                            (unsigned)flash_.totalBytes(),
                            (unsigned)flash_.maxBytes());
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
                            (unsigned)flash_.usedBytes(),
                            (unsigned)flash_.totalBytes(),
                            (unsigned)flash_.maxBytes(),
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
            Serial.print("[STATUS] SD mounted: "); Serial.println(sdOk_ ? "YES" : "NO");
            Serial.print("[STATUS] SD file: ");    Serial.println(sdDesiredPath_);
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
            snprintf(nameBuf, sizeof(nameBuf), "/logs/telemetry_%03lu.csv", (unsigned long)idx);
            sdDesiredPath_ = String(nameBuf);
            prefs.putString("sd_path", sdDesiredPath_);
            prefs.end();


            sd_.setDesiredPath(sdDesiredPath_);
            Serial.print("[SD] Rotated file to: "); Serial.println(sdDesiredPath_);
          #else
            Serial.println("[SD] disabled at compiletime.");
          #endif


          #if flashmemori
            flashPath_ = flash_.makeLogPath(++runId);
            flash_.ensureHeader(flashPath_);
            Serial.print("[FLASH] Rotated file to: "); Serial.println(flashPath_);
          #endif


        } else if (cmd.length()) {
          Serial.print("[CMD] Unknown: "); Serial.println(cmd);
          Serial.println("[CMD] Try: clear | status | print | new");
        }
      } else {
        if (serialBuf_.length() < 128) serialBuf_ += ch;
      }
    }
    #endif
  }


  uint32_t lastTick_ = 0;


  uint32_t loRaRetries_  = 0;
  uint32_t storageFails_ = 0;


  TelemetryRingBuffer loraBuf_;
  SensorSnapshot lastSens_;


  #if SDKort
    SdLogger sd_{SD_CS};
    bool sdOk_ = false;
    String sdDesiredPath_;
  #endif


  #if flashmemori
    FlashLogger flash_{FLASH_LOG_DIR, FLASH_MAX_BYTES};
    bool flashOk_ = false;
    String flashPath_;
  #endif


  #if LoRa
    LoRaLink lora_{LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY};
    bool loraOk_ = false;
  #endif


  String serialBuf_;
};


TelemetryApp app;


void setup() { app.begin(); }
void loop()  { app.tick(); }





