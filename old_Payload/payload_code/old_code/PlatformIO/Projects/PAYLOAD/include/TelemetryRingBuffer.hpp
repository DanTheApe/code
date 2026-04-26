#pragma once

#include "Config.hpp"

struct TelemetryMeasurement {
  uint16_t id;
  uint32_t t_ms;
  int32_t value;
};

struct TelemetryPacket {
  static const size_t MAX_MEASUREMENTS = 10;

  TelemetryMeasurement measurements[MAX_MEASUREMENTS];
  size_t count;

  TelemetryPacket() : count(0) {}

  bool add(uint16_t id, uint32_t tMs, int32_t value) {
    if (count >= MAX_MEASUREMENTS) return false;
    measurements[count].id = id;
    measurements[count].t_ms = tMs;
    measurements[count].value = value;
    count++;
    return true;
  }
};

class TelemetryRingBuffer {
public:
  static const size_t CAP = 10;

  TelemetryRingBuffer();

  void push(const TelemetryPacket& p);
  size_t size() const;
  const TelemetryPacket& peek(size_t idxFromOldest) const;
  void popN(size_t n);

private:
  TelemetryPacket buf_[CAP];
  size_t head_;
  size_t tail_;
  size_t count_;
};
