#pragma once

#include "Config.hpp"

class SdLogger {
public:
  explicit SdLogger(int csPin);

  bool begin();
  void maybeRemount(uint32_t nowMs);
  void setDesiredPath(const String& p);
  bool appendSticky(const String& line);
  void ensureHeader(const String& path);

  bool mounted() const;
  String currentPath() const;

private:
  int cs_;
  bool mounted_ = false;
  bool readyForAppends_ = false;
  String desiredPath_;
  uint32_t lastAttemptMs_ = 0;
  const uint32_t mountRetryMs_ = 1000;
};
