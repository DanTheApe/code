#pragma once

#include "Config.hpp"

struct TelemetrySample {
  uint32_t t_ms;
  int c6s;
};

class TelemetryRingBuffer {
public:
  static const size_t CAP = 10;

  void push(const TelemetrySample& s);
  size_t size() const;
  const TelemetrySample& peek(size_t idxFromOldest) const;
  void popN(size_t n);

private:
  TelemetrySample buf_[CAP];
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
};
