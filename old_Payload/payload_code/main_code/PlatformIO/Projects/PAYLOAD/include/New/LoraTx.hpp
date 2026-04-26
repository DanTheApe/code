#pragma once

// =============================================================================
//  LoraTx.hpp
//  Owns the SX1280 radio plus a small ring buffer of frames that failed to
//  transmit. The cycle is:
//    1. App reads sensors and calls send(frame, len).
//    2. send() tries one radio_.transmit(); on failure the frame is queued.
//    3. service() walks the queue once per tick, retrying a few entries.
//    4. After LORA_MAX_ATTEMPTS the frame is dropped (counted in dropped()).
// =============================================================================

#include "Config.hpp"

#if USE_LORA

struct PendingFrame {
  uint8_t data[LORA_FRAME_MAX];  // Raw bytes ready for the air.
  uint8_t len = 0;               // How many of those bytes are valid.
  uint8_t attempts = 0;          // Number of TX attempts already made.
};

class LoraTx {
public:
  LoraTx();

  // Input  : (none)
  // What   : powers up the SX1280, configures freq/bw/sf/cr/power, etc.
  // Output : true if the radio reports ready, false on any setup failure.
  bool begin();

  // Input  : data, len - one ready-to-transmit MAVLink frame.
  // What   : tries an immediate transmit; on failure, pushes onto the retry
  //          ring buffer so it will be retransmitted by service().
  // Output : true if the frame went out on this call, false if it was queued
  //          (or silently dropped because the radio is not ready).
  bool send(const uint8_t* data, uint8_t len);

  // Input  : maxAttemptsThisTick - upper bound on retries to try this call.
  // What   : iterates over the retry queue, attempting each frame once.
  //          Successful frames are removed; failed frames stay queued and
  //          their attempt count increments. Frames whose attempt count
  //          reaches LORA_MAX_ATTEMPTS are dropped.
  // Output : number of frames actually transmitted on this call.
  size_t service(uint8_t maxAttemptsThisTick);

  // Quick state probes used by the app for telemetry rows / serial print.
  size_t   queueSize()    const { return count_; }
  uint32_t totalSent()    const { return sent_; }
  uint32_t totalRetries() const { return retries_; }
  uint32_t totalDropped() const { return dropped_; }

private:
  // Push a frame onto the queue. If full, the oldest entry is overwritten and
  // the dropped counter is incremented.
  void enqueue_(const uint8_t* data, uint8_t len, uint8_t prevAttempts);

  // Pop the oldest queue entry, copying its contents into out.
  bool popOldest_(PendingFrame& out);

  Module* mod_ = nullptr;
  SX1280  radio_;
  bool    ready_ = false;

  PendingFrame queue_[LORA_RETRY_QUEUE];
  size_t head_  = 0;             // Next slot to write into.
  size_t tail_  = 0;             // Oldest slot still occupied.
  size_t count_ = 0;

  uint32_t sent_    = 0;
  uint32_t retries_ = 0;
  uint32_t dropped_ = 0;
};

#endif
