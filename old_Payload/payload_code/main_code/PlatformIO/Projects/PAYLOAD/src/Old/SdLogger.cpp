#include "Old/SdLogger.hpp"

SdLogger::SdLogger(int csPin) : cs_(csPin) {}

#if SDKort
bool SdLogger::begin() {
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

void SdLogger::maybeRemount(uint32_t nowMs) {
  if (mounted_) return;
  if (nowMs - lastAttemptMs_ < mountRetryMs_) return;
  lastAttemptMs_ = nowMs;
  begin();
  if (mounted_ && desiredPath_.length()) {
    ensureHeader(desiredPath_);
    #if WrightToSerial
      Serial.print("[SD] Logging to: ");
      Serial.println(desiredPath_);
    #endif
    readyForAppends_ = true;
  }
}

void SdLogger::setDesiredPath(const String& p) {
  desiredPath_ = p;
  if (mounted_) {
    SD.mkdir("/logs");
    ensureHeader(desiredPath_);
    readyForAppends_ = true;
    #if WrightToSerial
      Serial.print("[SD] Logging to: ");
      Serial.println(desiredPath_);
    #endif
  } else {
    readyForAppends_ = false;
  }
}

bool SdLogger::appendSticky(const String& line) {
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

void SdLogger::ensureHeader(const String& path) {
  File fr = SD.open(path, FILE_READ);
  bool needHeader = true;
  if (fr) {
    needHeader = (fr.size() == 0);
    fr.close();
  }
  if (needHeader) {
    File fw = SD.open(path, FILE_APPEND);
    if (fw) {
      fw.println(CSV_HEADER);
      fw.close();
    }
  }
}

bool SdLogger::mounted() const {
  return mounted_;
}

String SdLogger::currentPath() const {
  return desiredPath_;
}
#else
bool SdLogger::begin() { return false; }

void SdLogger::maybeRemount(uint32_t) {}

void SdLogger::setDesiredPath(const String&) {}

bool SdLogger::appendSticky(const String&) { return false; }

void SdLogger::ensureHeader(const String&) {}

bool SdLogger::mounted() const { return false; }

String SdLogger::currentPath() const { return String(); }
#endif
