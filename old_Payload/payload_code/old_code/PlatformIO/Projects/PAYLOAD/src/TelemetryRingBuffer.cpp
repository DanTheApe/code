#include "TelemetryRingBuffer.hpp"

TelemetryRingBuffer::TelemetryRingBuffer() : head_(0), tail_(0), count_(0) {}

void TelemetryRingBuffer::push(const TelemetryPacket& p) {
  if (count_ == CAP) {
    tail_ = (tail_ + 1) % CAP;
    count_--;
  }
  buf_[head_] = p;
  head_ = (head_ + 1) % CAP;
  count_++;
}

size_t TelemetryRingBuffer::size() const { return count_; }

const TelemetryPacket& TelemetryRingBuffer::peek(size_t idxFromOldest) const {
  size_t idx = (tail_ + idxFromOldest) % CAP;
  return buf_[idx];
}

void TelemetryRingBuffer::popN(size_t n) {
  if (n > count_) n = count_;
  tail_ = (tail_ + n) % CAP;
  count_ -= n;
}
