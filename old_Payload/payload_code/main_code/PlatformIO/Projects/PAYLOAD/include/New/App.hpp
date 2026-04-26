#pragma once

// =============================================================================
//  App.hpp
//  Top-level orchestrator. Runs the per-second telemetry cycle:
//    1. Drain the LoRa retry queue.
//    2. Refresh sensor readings.
//    3. Build one MAVLink frame per sensor and hand it to the radio.
//    4. Format a CSV row and append it to SD + flash.
//    5. Process serial commands (status / clear / dump / new).
// =============================================================================

#include "Config.hpp"
#include "Sensors.hpp"
#include "Storage.hpp"
#if USE_LORA
  #include "LoraTx.hpp"
#endif

class App {
public:
  // Input  : (none)
  // What   : sets up serial, buses, NVS run id, all sensors, storage, radio.
  // Output : (none)
  void begin();

  // Input  : (none)
  // What   : called from loop(); polls sensors, runs the TX cycle, services
  //          the LoRa retry queue, processes any serial commands.
  // Output : (none)
  void tick();

private:
  // Input  : nowMs - current millis() timestamp.
  // What   : reads each sensor and sends one MAVLink frame per reading,
  //          one logical packet at a time, then logs a CSV row.
  // Output : (none)
  void runTxCycle_(uint32_t nowMs);

  // Input  : nowMs, accel, gyro, mag, baro, geig, lt, lv - latest readings.
  // What   : formats one CSV record carrying every sensor's most recent value.
  // Output : the formatted CSV line (no trailing newline).
  String buildCsvRow_(uint32_t nowMs,
                      const AccelReading& a,
                      const GyroReading&  g,
                      const MagReading&   m,
                      const BaroReading&  b,
                      const GeigerReading& gc,
                      const LoraTempReading& lt,
                      const LoraVoltReading& lv);

  // Input  : (none)
  // What   : reads any user input on Serial and runs `status`, `print`,
  //          `clear`, `new`, or `last` commands.
  // Output : (none)
  void processSerialCommands_();

  // Helper that submits one prebuilt frame to the radio (when USE_LORA).
  void sendFrame_(const uint8_t* buf, size_t len);

  // Input  : label - short tag like "ACCEL " / "BARO_P"; body - decoded
  //          values text; bytes/len - the raw frame about to be transmitted.
  // What   : prints "[TX] LABEL  BODY  (N bytes)" to serial, plus an
  //          optional hex dump if PRINT_TX_HEX is enabled.
  // Output : (none)
  void printTx_(const char* label, const char* body,
                const uint8_t* bytes, size_t len);

  Sensors  sensors_;
  Storage  storage_;
#if USE_LORA
  LoraTx   lora_;
#endif

  uint32_t lastTxCycleMs_      = 0;
  uint32_t lastSensorPollMs_   = 0;
  uint32_t lastHeartbeatMs_    = 0;
  String   serialBuf_;
};
