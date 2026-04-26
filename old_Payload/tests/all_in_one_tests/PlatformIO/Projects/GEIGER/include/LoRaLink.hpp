#pragma once

#include "Config.hpp"

#if LoRa
class LoRaLink {
public:
  struct TxResult {
    int status;
    uint8_t retriesUsed;
  };

  LoRaLink(int nss, int dio1, int rst, int busy);

  bool begin();
  TxResult transmitWithRetry(String& payload, uint32_t& retryAccumulator);
  TxResult transmitWithRetry(uint8_t* data, size_t len, uint32_t& retryAccumulator);

private:
  bool fail(const char* what);

  Module* mod_;
  SX1280 radio_;
};
#endif
