#include "Config.hpp"
#include "TelemetryApp.hpp"

Preferences prefs;
uint32_t runId = 0;
RTC_DATA_ATTR uint32_t g_boot_reason_raw = 0;
volatile uint32_t GEIGER_PULSE_COUNT = 0;

const char* CSV_HEADER =
  "runId,t_ms,c/6s,"
  "p_Pa,t_cC,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps,mx_uT10,my_uT10,mz_uT10,sOkMask,"
  "LoRaStatus,Retries,StorageOK,LoRaRetriesTotal,StorageFailsTotal";

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT (RTC IO)";
    case ESP_RST_SW:        return "SW (esp_restart)";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

uint16_t rand16() { return static_cast<uint16_t>(esp_random() & 0xFFFF); }

void IRAM_ATTR geigerISR() { GEIGER_PULSE_COUNT++; }

TelemetryApp app;

void setup() { app.begin(); }
void loop() { app.tick(); }
