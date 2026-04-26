#include "Old/LoRaLink.hpp"

#if LoRa
LoRaLink::LoRaLink(int nss, int dio1, int rst, int busy)
  : mod_(new Module(nss, dio1, rst, busy, SPI, SPISettings(8000000, MSBFIRST, SPI_MODE0))),
    radio_(mod_) {}

bool LoRaLink::begin() {
  Serial.println("[LoRa] Begin");
  int state = radio_.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] begin() failed: %d\n", state);
    return false;
  }
  if (radio_.setFrequency(LORA_FREQ_MHZ) != RADIOLIB_ERR_NONE) return fail("setFrequency");
  if (radio_.setBandwidth(LORA_BW_KHZ) != RADIOLIB_ERR_NONE) return fail("setBandwidth");
  if (radio_.setSpreadingFactor(LORA_SF) != RADIOLIB_ERR_NONE) return fail("setSpreadingFactor");
  if (radio_.setCodingRate(LORA_CR) != RADIOLIB_ERR_NONE) return fail("setCodingRate");
  if (radio_.setOutputPower(LORA_TX_PWR) != RADIOLIB_ERR_NONE) return fail("setOutputPower");
  radio_.setCRC(true);
  radio_.setWhitening(true);
  Serial.println("[LoRa] Ready");
  return true;
}

LoRaLink::TxResult LoRaLink::transmitWithRetry(String& payload, uint32_t& retryAccumulator) {
  TxResult r{RADIOLIB_ERR_UNKNOWN, 0};
  for (uint8_t attempt = 0; attempt <= MAX_LORA_RETRIES; ++attempt) {
    int s = radio_.transmit(payload);
    if (s == RADIOLIB_ERR_NONE) {
      r.status = s;
      r.retriesUsed = attempt;
      if (attempt > 0) retryAccumulator += attempt;
      return r;
    }
    if (attempt == MAX_LORA_RETRIES) {
      r.status = s;
      r.retriesUsed = attempt;
      retryAccumulator += attempt;
      return r;
    }
    uint16_t jitter = rand16() % 25;
    uint16_t backoff = (BASE_BACKOFF_MS << attempt) + jitter;
    delay(backoff);
  }
  return r;
}

LoRaLink::TxResult LoRaLink::transmitWithRetry(uint8_t* data, size_t len, uint32_t& retryAccumulator) {
  TxResult r{RADIOLIB_ERR_UNKNOWN, 0};
  for (uint8_t attempt = 0; attempt <= MAX_LORA_RETRIES; ++attempt) {
    int s = radio_.transmit(data, len);
    if (s == RADIOLIB_ERR_NONE) {
      r.status = s;
      r.retriesUsed = attempt;
      if (attempt > 0) retryAccumulator += attempt;
      return r;
    }
    if (attempt == MAX_LORA_RETRIES) {
      r.status = s;
      r.retriesUsed = attempt;
      retryAccumulator += attempt;
      return r;
    }
    uint16_t jitter = rand16() % 25;
    uint16_t backoff = (BASE_BACKOFF_MS << attempt) + jitter;
    delay(backoff);
  }
  return r;
}

bool LoRaLink::fail(const char* what) {
  Serial.print("[LoRa] ");
  Serial.print(what);
  Serial.println(" failed");
  return false;
}
#endif
