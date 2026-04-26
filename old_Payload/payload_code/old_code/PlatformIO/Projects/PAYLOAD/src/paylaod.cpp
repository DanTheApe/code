#include "Config.hpp"
#include "TelemetryApp.hpp"
#include "rom/rtc.h"

Preferences prefs;
uint32_t runId = 0;
RTC_DATA_ATTR uint32_t g_boot_reason_raw = 0;
volatile uint32_t GEIGER_PULSE_COUNT = 0;

const char* CSV_HEADER =
  "runId,t_ms,c/1s,"
  "p_Pa,t_cC,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps,mx_uT10,my_uT10,mz_uT10,sOkMask,"
  "LoRaStatus,Retries,StorageOK,LoRaRetriesTotal,StorageFailsTotal";

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
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

uint32_t readRtcResetReasonRaw() {
  return static_cast<uint32_t>(rtc_get_reset_reason(0));
}

const char* rtcResetReasonStr(uint32_t raw) {
  switch (raw) {
    case 1:  return "POWERON_RESET";
    case 3:  return "SW_RESET";
    case 4:  return "OWDT_RESET";
    case 5:  return "DEEPSLEEP_RESET";
    case 6:  return "SDIO_RESET";
    case 7:  return "TG0WDT_SYS_RESET";
    case 8:  return "TG1WDT_SYS_RESET";
    case 9:  return "RTCWDT_SYS_RESET";
    case 10: return "INTRUSION_RESET";
    case 11: return "TGWDT_CPU_RESET";
    case 12: return "SW_CPU_RESET";
    case 13: return "RTCWDT_CPU_RESET";
    case 14: return "EXT_CPU_RESET";
    case 15: return "RTCWDT_BROWN_OUT_RESET";
    case 16: return "RTCWDT_RTC_RESET";
    case 17: return "MWDT1_CPU_RESET";
    case 18: return "SUPER_WDT_SYS_RESET";
    case 19: return "CLK_GLITCH_SYS_RESET";
    case 20: return "EFUSE_CRC_CORE_RESET";
    case 21: return "USB_UART_CORE_RESET";
    case 22: return "USB_JTAG_CORE_RESET";
    case 23: return "PWR_GLITCH_CORE_RESET";
    default: return "UNKNOWN";
  }
}

uint16_t rand16() { return static_cast<uint16_t>(esp_random() & 0xFFFF); }

void IRAM_ATTR geigerISR() { GEIGER_PULSE_COUNT++; }

TelemetryApp app;

void setup() { app.begin(); }
void loop() { app.tick(); }
