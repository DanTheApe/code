#include "New/App.hpp"
#if USE_LORA
  #include "New/MavlinkFrames.hpp"
#endif

// =============================================================================
//  App.cpp
//  Top-level orchestrator. setup() -> begin(), loop() -> tick().
//  See App.hpp for the per-cycle outline.
// =============================================================================

// =============================================================================
//  begin
// =============================================================================
void App::begin() {
#if USE_SERIAL
  Serial.begin(115200);
  for (uint32_t t = millis(); millis() - t < 200;) { /* small settling */ }
  Serial.println();
  Serial.println("==== PAYLOAD (NEW) booting ====");
#endif

  // Bring up shared buses once for everyone.
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  // Drive every SPI chip-select high BEFORE first transfer.  The IMU has an
  // internal pull-down on CS, so leaving it floating would corrupt SD init.
#if USE_IMU
  pinMode(PIN_IMU_CS, OUTPUT); digitalWrite(PIN_IMU_CS, HIGH);
#endif
#if USE_LORA
  pinMode(PIN_LORA_CS, OUTPUT); digitalWrite(PIN_LORA_CS, HIGH);
#endif
#if USE_SD
  pinMode(PIN_SD_CS, OUTPUT);  digitalWrite(PIN_SD_CS, HIGH);
#endif

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  // Load / increment the cold-boot run id from NVS.
  prefs.begin("payload", false);
  runId = prefs.getUInt("run", 0) + 1;
  prefs.putUInt("run", runId);
  prefs.end();
#if USE_SERIAL
  Serial.printf("[APP] runId=%lu\n", static_cast<unsigned long>(runId));
#endif

  // Geiger ISRs.
#if USE_GEIGER
  pinMode(PIN_GEIGER_1, INPUT);
  pinMode(PIN_GEIGER_2, INPUT);
  attachInterrupt(PIN_GEIGER_1, geigerCh1Isr, RISING);
  attachInterrupt(PIN_GEIGER_2, geigerCh2Isr, RISING);
#endif

  storage_.begin(runId);
  sensors_.begin();

#if USE_LORA
  bool ok = lora_.begin();
  if (!ok) {
#if USE_SERIAL
    Serial.println("[APP] LoRa init failed (continuing without radio)");
#endif
  }
#endif

  uint32_t now = millis();
  lastTxCycleMs_    = now;
  lastSensorPollMs_ = now;
  lastHeartbeatMs_  = now;
  sensors_.poll(now);

#if USE_SERIAL
  Serial.println("[APP] ready. Commands: status | print | clear | new | last");
#endif
}

// =============================================================================
//  tick
// =============================================================================
void App::tick() {
  uint32_t now = millis();

  processSerialCommands_();

  // Drain a few retry frames every loop so the queue does not grow.
#if USE_LORA
  lora_.service(/*maxAttemptsThisTick=*/2);
#endif

  // Refresh accel/gyro/mag/baro at SENSOR_POLL_INTERVAL_MS cadence.
  if (static_cast<uint32_t>(now - lastSensorPollMs_) >= SENSOR_POLL_INTERVAL_MS) {
    sensors_.poll(now);
    lastSensorPollMs_ = now;
  }

  // Run a full TX + log cycle every TX_CYCLE_INTERVAL_MS.
  if (static_cast<uint32_t>(now - lastTxCycleMs_) >= TX_CYCLE_INTERVAL_MS) {
    runTxCycle_(now);
    lastTxCycleMs_ = now;
  }
}

// =============================================================================
//  runTxCycle_  -  the heart of the firmware.
//  One MAVLink frame per sensor, one LoRa packet per frame.
// =============================================================================
void App::runTxCycle_(uint32_t nowMs) {
  AccelReading    a  = sensors_.lastAccel();
  GyroReading     g  = sensors_.lastGyro();
  MagReading      m  = sensors_.lastMag();
  BaroReading     b  = sensors_.lastBaro();
  GeigerReading   gc = sensors_.takeGeiger(nowMs);
  LoraTempReading lt = sensors_.readLoraTemp(nowMs);
  LoraVoltReading lv = sensors_.readLoraVolt(nowMs);

#if USE_LORA
  uint8_t buf[LORA_FRAME_MAX];
  size_t  n;

  // Heartbeat every HEARTBEAT_INTERVAL_MS so a ground station knows we are alive.
  if (static_cast<uint32_t>(nowMs - lastHeartbeatMs_) >= HEARTBEAT_INTERVAL_MS) {
    n = MavlinkFrames::buildHeartbeat(buf);
    printTx_("HEARTBEAT", "(no payload)", buf, n);
    sendFrame_(buf, n);
    lastHeartbeatMs_ = nowMs;
  }

  // Each sensor reading goes out as its own MAVLink frame / LoRa packet.
  char body[80];
  if (a.ok)  {
    n = MavlinkFrames::buildAccel(buf, a);
    snprintf(body, sizeof(body), "t=%lu ms  x=%d y=%d z=%d mg",
             (unsigned long)a.t_ms, a.x_mg, a.y_mg, a.z_mg);
    printTx_("ACCEL ", body, buf, n);
    sendFrame_(buf, n);
  }
  if (g.ok)  {
    n = MavlinkFrames::buildGyro(buf, g);
    snprintf(body, sizeof(body), "t=%lu ms  x=%d y=%d z=%d mrad/s",
             (unsigned long)g.t_ms, g.x_mrad_s, g.y_mrad_s, g.z_mrad_s);
    printTx_("GYRO  ", body, buf, n);
    sendFrame_(buf, n);
  }
  if (m.ok)  {
    n = MavlinkFrames::buildMag(buf, m);
    snprintf(body, sizeof(body), "t=%lu ms  x=%d y=%d z=%d mgauss",
             (unsigned long)m.t_ms, m.x_mgauss, m.y_mgauss, m.z_mgauss);
    printTx_("MAG   ", body, buf, n);
    sendFrame_(buf, n);
  }
  if (b.ok)  {
    n = MavlinkFrames::buildBaroPressure(buf, b);
    snprintf(body, sizeof(body), "t=%lu ms  press=%.2f hPa",
             (unsigned long)b.t_ms, b.pressure_hPa);
    printTx_("BARO_P", body, buf, n);
    sendFrame_(buf, n);
  }
  if (b.ok)  {
    n = MavlinkFrames::buildBaroTemp(buf, b);
    snprintf(body, sizeof(body), "t=%lu ms  temp=%d cC",
             (unsigned long)b.t_ms, b.temperature_cC);
    printTx_("BARO_T", body, buf, n);
    sendFrame_(buf, n);
  }
  if (gc.ok) {
    n = MavlinkFrames::buildGeiger(buf, gc, 1);
    snprintf(body, sizeof(body), "t=%lu ms  counts=%lu",
             (unsigned long)gc.t_ms, (unsigned long)gc.counts1);
    printTx_("GEIG_1", body, buf, n);
    sendFrame_(buf, n);
  }
  if (gc.ok) {
    n = MavlinkFrames::buildGeiger(buf, gc, 2);
    snprintf(body, sizeof(body), "t=%lu ms  counts=%lu",
             (unsigned long)gc.t_ms, (unsigned long)gc.counts2);
    printTx_("GEIG_2", body, buf, n);
    sendFrame_(buf, n);
  }
  if (lt.ok) {
    n = MavlinkFrames::buildLoraTemp(buf, lt);
    snprintf(body, sizeof(body), "t=%lu ms  temp=%d cC",
             (unsigned long)lt.t_ms, lt.t_cC);
    printTx_("LORA_T", body, buf, n);
    sendFrame_(buf, n);
  }
  if (lv.ok) {
    n = MavlinkFrames::buildLoraVolt(buf, lv);
    snprintf(body, sizeof(body), "t=%lu ms  volt=%u mV",
             (unsigned long)lv.t_ms, lv.mV);
    printTx_("LORA_V", body, buf, n);
    sendFrame_(buf, n);
  }
#endif

  // CSV row mirrors every sensor value into SD/flash.
  String row = buildCsvRow_(nowMs, a, g, m, b, gc, lt, lv);
  storage_.writeRow(row);

#if USE_SERIAL
  Serial.print("[ROW] "); Serial.println(row);
#if USE_LORA
  Serial.printf("[LoRa] sent=%lu retries=%lu drops=%lu queued=%u\n",
                static_cast<unsigned long>(lora_.totalSent()),
                static_cast<unsigned long>(lora_.totalRetries()),
                static_cast<unsigned long>(lora_.totalDropped()),
                static_cast<unsigned>(lora_.queueSize()));
#endif
#endif
}

// =============================================================================
//  buildCsvRow_
// =============================================================================
String App::buildCsvRow_(uint32_t nowMs,
                         const AccelReading& a,
                         const GyroReading&  g,
                         const MagReading&   m,
                         const BaroReading&  b,
                         const GeigerReading& gc,
                         const LoraTempReading& lt,
                         const LoraVoltReading& lv) {
  String r;
  r.reserve(280);
  r += "Run_ID = "          + String(runId) + "|";
  r += "Now_Ms = "          + String(nowMs) + "|";
  r += "Accsloration_mg = " + String(a.ok ? a.x_mg : 0)     + "," + String(a.ok ? a.y_mg : 0)     + "," + String(a.ok ? a.z_mg : 0)     + "|";
  r += "Gyro_mrad_s = "     + String(g.ok ? g.x_mrad_s : 0) + "," + String(g.ok ? g.y_mrad_s : 0) + "," + String(g.ok ? g.z_mrad_s : 0) + "|";
  r += "Mag_mgauss = "      + String(m.ok ? m.x_mgauss : 0) + "," + String(m.ok ? m.y_mgauss : 0) + "," + String(m.ok ? m.z_mgauss : 0) + "|";
  r += "Pressure_hPa = "    + String(b.ok ? b.pressure_hPa : 0.0f, 2) + "|";
  r += "Temperature_cC = "  + String(b.ok ? b.temperature_cC : 0)     + "|";
  r += "Geiger_1 = "        + String(static_cast<unsigned long>(gc.counts1)) + "|";
  r += "Geiger_2 = "        + String(static_cast<unsigned long>(gc.counts2)) + "|";
  r += "LoRa_Temp_cC = "    + String(lt.ok ? lt.t_cC : 0) + "|";
  r += "LoRa_Volt_mV = "    + String(lv.ok ? lv.mV   : 0) + "|";
#if USE_LORA
  r += "LoRa_Retries = "    + String(static_cast<unsigned long>(lora_.totalRetries())) + "|";
  r += "LoRa_Drops = "      + String(static_cast<unsigned long>(lora_.totalDropped())) + "|";
#else
  r += "LoRa_Retries = 0|";
  r += "LoRa_Drops = 0|";
#endif
  r += "Storage_Fails = "   + String(static_cast<unsigned long>(storage_.failures()));
  return r;
}

// =============================================================================
//  printTx_
// =============================================================================
void App::printTx_(const char* label, const char* body,
                   const uint8_t* bytes, size_t len) {
#if USE_SERIAL && PRINT_TX
  Serial.printf("[TX] %s  %s  (%u bytes)\n",
                label, body, static_cast<unsigned>(len));
#if PRINT_TX_HEX
  Serial.print("[TX]   bytes:");
  for (size_t i = 0; i < len; ++i) {
    Serial.printf(" %02X", bytes[i]);
  }
  Serial.println();
#else
  (void)bytes;
#endif
#else
  (void)label; (void)body; (void)bytes; (void)len;
#endif
}

// =============================================================================
//  sendFrame_
// =============================================================================
void App::sendFrame_(const uint8_t* buf, size_t len) {
#if USE_LORA
  if (len == 0 || len > LORA_FRAME_MAX) return;
  lora_.send(buf, static_cast<uint8_t>(len));
#else
  (void)buf; (void)len;
#endif
}

// =============================================================================
//  processSerialCommands_
// =============================================================================
void App::processSerialCommands_() {
#if USE_SERIAL
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      String cmd = serialBuf_;
      serialBuf_.clear();
      cmd.trim();
      cmd.toLowerCase();

      if (cmd == "status") {
        Serial.printf("[STAT] runId=%lu sd=%d flash=%d storageFails=%lu\n",
                      static_cast<unsigned long>(runId),
                      storage_.sdMounted() ? 1 : 0,
                      storage_.flashMounted() ? 1 : 0,
                      static_cast<unsigned long>(storage_.failures()));
#if USE_LORA
        Serial.printf("[STAT] lora sent=%lu retries=%lu drops=%lu queued=%u\n",
                      static_cast<unsigned long>(lora_.totalSent()),
                      static_cast<unsigned long>(lora_.totalRetries()),
                      static_cast<unsigned long>(lora_.totalDropped()),
                      static_cast<unsigned>(lora_.queueSize()));
#endif
      } else if (cmd == "print") {
        storage_.dumpFlashToSerial();
      } else if (cmd == "clear") {
        storage_.wipeFlash();
        Serial.println("[CMD] flash wiped");
      } else if (cmd == "new") {
        // Bump runId so the next written row goes into a new file.
        prefs.begin("payload", false);
        runId = prefs.getUInt("run", 0) + 1;
        prefs.putUInt("run", runId);
        prefs.end();
        storage_.begin(runId);
        Serial.printf("[CMD] new run id=%lu\n", static_cast<unsigned long>(runId));
      } else if (cmd == "last") {
        AccelReading a = sensors_.lastAccel();
        GyroReading  g = sensors_.lastGyro();
        MagReading   m = sensors_.lastMag();
        BaroReading  b = sensors_.lastBaro();
        Serial.printf("[LAST] acc(mg)=%d,%d,%d  gyro(mrad/s)=%d,%d,%d  "
                      "mag(mG)=%d,%d,%d  baro=%.2fhPa,%dcC\n",
                      a.x_mg, a.y_mg, a.z_mg,
                      g.x_mrad_s, g.y_mrad_s, g.z_mrad_s,
                      m.x_mgauss, m.y_mgauss, m.z_mgauss,
                      b.pressure_hPa, b.temperature_cC);
      } else if (cmd.length()) {
        Serial.print("[CMD] unknown: "); Serial.println(cmd);
        Serial.println("[CMD] try: status | print | clear | new | last");
      }
    } else {
      if (serialBuf_.length() < 128) serialBuf_ += ch;
    }
  }
#endif
}
