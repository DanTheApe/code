#pragma once

#include "Config.hpp"
#include "TelemetryRingBuffer.hpp"
#include "SdLogger.hpp"
#include "FlashLogger.hpp"
#include "GeigerCounter.hpp"
#include "LoRaLink.hpp"

class TelemetryApp {
public:
  void begin();
  void tick();

private:
  String buildLoraPayload3();
  void processSerialCommands();

  uint32_t lastTick_ = 0;
  uint32_t loRaRetries_ = 0;
  uint32_t storageFails_ = 0;

  TelemetryRingBuffer loraBuf_;
  SensorSnapshot lastSens_;

  #if SDKort
    SdLogger sd_{SD_CS};
    bool sdOk_ = false;
    String sdDesiredPath_;
  #endif

  #if flashmemori
    FlashLogger flash_{FLASH_LOG_DIR, FLASH_MAX_BYTES};
    bool flashOk_ = false;
    String flashPath_;
  #endif

  #if LoRa
    LoRaLink lora_{LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY};
    bool loraOk_ = false;
  #endif

  String serialBuf_;
};
