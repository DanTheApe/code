#include "New/Storage.hpp"

// =============================================================================
//  Storage.cpp
//  CSV row mirror across SD and LittleFS.  Either backend can be missing or
//  fail mid-flight; the other keeps logging.
// =============================================================================

bool Storage::begin(uint32_t run) {
  bool any = false;

#if USE_SD
  sdOk_ = SD.begin(PIN_SD_CS, SPI, 8000000);
  if (sdOk_) {
    SD.mkdir(SD_LOG_DIR);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s/run_%06lu.csv", SD_LOG_DIR,
             static_cast<unsigned long>(run));
    sdPath_ = String(buf);
    sdEnsureHeader_(sdPath_);
#if USE_SERIAL
    Serial.print("[STOR] SD log: "); Serial.println(sdPath_);
#endif
    any = true;
  } else {
#if USE_SERIAL
    Serial.println("[STOR] SD mount failed");
#endif
  }
#endif

#if USE_FLASH
  flashOk_ = LittleFS.begin(true);
  if (flashOk_) {
    LittleFS.mkdir(FLASH_LOG_DIR);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s/run_%06lu.csv", FLASH_LOG_DIR,
             static_cast<unsigned long>(run));
    flashPath_ = String(buf);
    flashEnsureHeader_(flashPath_);
#if USE_SERIAL
    Serial.printf("[STOR] LittleFS used=%u total=%u\n",
                  static_cast<unsigned>(LittleFS.usedBytes()),
                  static_cast<unsigned>(LittleFS.totalBytes()));
    Serial.print("[STOR] Flash log: "); Serial.println(flashPath_);
#endif
    any = true;
  } else {
#if USE_SERIAL
    Serial.println("[STOR] LittleFS mount failed");
#endif
  }
#endif

  return any;
}

// =============================================================================
//  writeRow
// =============================================================================
void Storage::writeRow(const String& line) {
#if USE_SD
  if (sdOk_ && sdPath_.length()) {
    File f = SD.open(sdPath_, FILE_APPEND);
    if (!f || f.println(line) == 0) {
      fails_++;
      sdOk_ = false;
#if USE_SERIAL
      Serial.println("[STOR] SD write failed");
#endif
    }
    if (f) f.close();
  }
#endif

#if USE_FLASH
  if (flashOk_ && flashPath_.length()) {
    if (LittleFS.usedBytes() >= FLASH_QUOTA_B) {
      fails_++;
      flashOk_ = false;
#if USE_SERIAL
      Serial.println("[STOR] flash quota reached");
#endif
    } else {
      File f = LittleFS.open(flashPath_, FILE_APPEND);
      if (!f || f.println(line) == 0) {
        fails_++;
#if USE_SERIAL
        Serial.println("[STOR] flash write failed");
#endif
      }
      if (f) f.close();
    }
  }
#endif
}

// =============================================================================
//  dumpFlashToSerial / wipeFlash
// =============================================================================
void Storage::dumpFlashToSerial() {
#if USE_FLASH && USE_SERIAL
  if (!flashOk_) { Serial.println("[STOR] flash not mounted"); return; }
  Serial.println("[STOR] BEGIN flash dump");
  flashPrintRecursive_(FLASH_LOG_DIR);
  Serial.println("[STOR] END flash dump");
#endif
}

void Storage::wipeFlash() {
#if USE_FLASH
  if (!flashOk_) return;
#if USE_SERIAL
  Serial.println("[STOR] wiping flash logs");
#endif
  flashDeleteRecursive_(FLASH_LOG_DIR);
  LittleFS.mkdir(FLASH_LOG_DIR);
  if (flashPath_.length()) flashEnsureHeader_(flashPath_);
#endif
}

// =============================================================================
//  Internal helpers
// =============================================================================
#if USE_SD
void Storage::sdEnsureHeader_(const String& path) {
  File f = SD.open(path, FILE_READ);
  bool need = true;
  if (f) { need = (f.size() == 0); f.close(); }
  if (need) {
    File w = SD.open(path, FILE_APPEND);
    if (w) { w.println(CSV_HEADER); w.close(); }
  }
}
#endif

#if USE_FLASH
void Storage::flashEnsureHeader_(const String& path) {
  File f = LittleFS.open(path, FILE_READ);
  bool need = true;
  if (f) { need = (f.size() == 0); f.close(); }
  if (need) {
    File w = LittleFS.open(path, FILE_APPEND);
    if (w) { w.println(CSV_HEADER); w.close(); }
  }
}

void Storage::flashPrintRecursive_(const char* path) {
#if USE_SERIAL
  File root = LittleFS.open(path);
  if (!root) return;
  if (!root.isDirectory()) { root.close(); return; }
  File f = root.openNextFile();
  while (f) {
    String full = f.path() ? String(f.path()) : (String(path) + "/" + f.name());
    if (f.isDirectory()) { f.close(); flashPrintRecursive_(full.c_str()); }
    else {
      Serial.print("[FILE] "); Serial.print(full);
      Serial.print(" ("); Serial.print(static_cast<unsigned>(f.size())); Serial.println(" b)");
      uint8_t buf[256];
      while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        if (n == 0) break;
        Serial.write(buf, n);
      }
      Serial.println();
      f.close();
    }
    f = root.openNextFile();
  }
  root.close();
#else
  (void)path;
#endif
}

void Storage::flashDeleteRecursive_(const char* path) {
  File root = LittleFS.open(path);
  if (!root) { LittleFS.mkdir(path); return; }
  if (!root.isDirectory()) {
    root.close();
    LittleFS.remove(path);
    return;
  }
  File f = root.openNextFile();
  while (f) {
    String full = f.path() ? String(f.path()) : (String(path) + "/" + f.name());
    if (f.isDirectory()) { f.close(); flashDeleteRecursive_(full.c_str()); }
    else { f.close(); LittleFS.remove(full.c_str()); }
    f = root.openNextFile();
  }
  root.close();
}
#endif
