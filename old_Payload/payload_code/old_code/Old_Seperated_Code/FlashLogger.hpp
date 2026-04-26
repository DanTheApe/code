#pragma once

#include "Config.hpp"

class FlashLogger {
public:
  FlashLogger(const char* dir, size_t maxBytes);

  bool begin();
  void wipeAllLogs();
  void printAllLogsToSerial();

  String makeLogPath(uint32_t run);
  void ensureHeader(const String& path);
  bool append(const String& path, const String& line);

  bool isFull() const;
  size_t usedBytes() const;
  size_t totalBytes() const;
  size_t maxBytes() const;

private:
  void deleteRecursive(const char* path);
  void printRecursive(const char* path);

  const char* dir_;
  size_t maxBytes_;
  bool stopped_ = false;
};
