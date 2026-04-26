#include "Old/FlashLogger.hpp"

FlashLogger::FlashLogger(const char* dir, size_t maxBytes)
  : dir_(dir), maxBytes_(maxBytes) {}

#if flashmemori
bool FlashLogger::begin() {
  if (!LittleFS.begin(true)) {
    #if WrightToSerial
      Serial.println("[FLASH] LittleFS mount failed");
    #endif
    return false;
  }
  LittleFS.mkdir(dir_);
  return true;
}

void FlashLogger::wipeAllLogs() {
  #if WrightToSerial
    Serial.println("[FLASH] Wiping logs (Serial command)...");
  #endif
  deleteRecursive(dir_);
}

void FlashLogger::printAllLogsToSerial() {
  #if WrightToSerial
    Serial.println("[FLASH]\tBEGIN PRINT ALL LOGS\t");
    printRecursive(dir_);
    Serial.println("[FLASH]\tEND PRINT ALL LOGS\t");
  #endif
}

String FlashLogger::makeLogPath(uint32_t run) {
  char buf[48];
  snprintf(buf, sizeof(buf), "%s/run_%06lu.csv", dir_, static_cast<unsigned long>(run));
  return String(buf);
}

void FlashLogger::ensureHeader(const String& path) {
  File f = LittleFS.open(path, FILE_READ);
  bool needHeader = true;
  if (f) {
    needHeader = (f.size() == 0);
    f.close();
  }
  if (needHeader) {
    File w = LittleFS.open(path, FILE_APPEND);
    if (w) {
      w.println(CSV_HEADER);
      w.close();
    }
  }
}

bool FlashLogger::append(const String& path, const String& line) {
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

bool FlashLogger::isFull() const { return LittleFS.usedBytes() >= maxBytes_; }
size_t FlashLogger::usedBytes() const { return LittleFS.usedBytes(); }
size_t FlashLogger::totalBytes() const { return LittleFS.totalBytes(); }
size_t FlashLogger::maxBytes() const { return maxBytes_; }

void FlashLogger::deleteRecursive(const char* path) {
  File root = LittleFS.open(path);
  if (!root) {
    LittleFS.mkdir(path);
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
        else full = String(path) + "/" + name;
      }
      if (f.isDirectory()) {
        f.close();
        deleteRecursive(full.c_str());
      } else {
        f.close();
        LittleFS.remove(full.c_str());
      }
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

void FlashLogger::printRecursive(const char* path) {
  File root = LittleFS.open(path);
  if (!root) {
    #if WrightToSerial
      Serial.print("[FLASH] Path not found: ");
      Serial.println(path);
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
        else full = String(path) + "/" + name;
      }
      if (f.isDirectory()) {
        f.close();
        printRecursive(full.c_str());
      } else {
        size_t sz = f.size();
        #if WrightToSerial
          Serial.println();
          Serial.print("[FLASH]\tBEGIN FILE: ");
          Serial.print(full);
          Serial.print(" (");
          Serial.print(static_cast<unsigned>(sz));
          Serial.println(" bytes)\t");
        #endif
        const size_t BUF_SZ = 512;
        uint8_t buf[BUF_SZ];
        while (f.available()) {
          size_t n = f.read(buf, BUF_SZ);
          if (n > 0) {
            Serial.write(buf, n);
          } else {
            break;
          }
        }
        f.close();
        #if WrightToSerial
          Serial.println();
          Serial.print("[FLASH]\tEND FILE: ");
          Serial.println(full);
        #endif
      }
      f = root.openNextFile();
    }
    root.close();
  } else {
    size_t sz = root.size();
    #if WrightToSerial
      Serial.println();
      Serial.print("[FLASH]\tBEGIN FILE: ");
      Serial.print(path);
      Serial.print(" (");
      Serial.print(static_cast<unsigned>(sz));
      Serial.println(" bytes)\t");
    #endif
    const size_t BUF_SZ = 512;
    uint8_t buf[BUF_SZ];
    while (root.available()) {
      size_t n = root.read(buf, BUF_SZ);
      if (n > 0) {
        Serial.write(buf, n);
      } else {
        break;
      }
    }
    root.close();
    #if WrightToSerial
      Serial.println();
      Serial.print("[FLASH]\tEND FILE: ");
      Serial.println(path);
    #endif
  }
}
#else
bool FlashLogger::begin() { return false; }
void FlashLogger::wipeAllLogs() {}
void FlashLogger::printAllLogsToSerial() {}
String FlashLogger::makeLogPath(uint32_t) { return String(); }
void FlashLogger::ensureHeader(const String&) {}
bool FlashLogger::append(const String&, const String&) { return false; }
bool FlashLogger::isFull() const { return true; }
size_t FlashLogger::usedBytes() const { return 0; }
size_t FlashLogger::totalBytes() const { return 0; }
size_t FlashLogger::maxBytes() const { return 0; }
void FlashLogger::deleteRecursive(const char*) {}
void FlashLogger::printRecursive(const char*) {}
#endif
