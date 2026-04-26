#include "New/Config.hpp"
#include "New/App.hpp"

// =============================================================================
//  main.cpp
//  Translation unit that owns every shared global, the geiger ISRs, and the
//  Arduino setup()/loop() entry points.
// =============================================================================

// Shared globals declared in Config.hpp.
Preferences prefs;
uint32_t    runId = 0;
volatile uint32_t geigerCh1Pulses = 0;
volatile uint32_t geigerCh2Pulses = 0;

// CSV header — must match the column order produced by App::buildCsvRow_.
const char* CSV_HEADER =
  "runId,t_ms,"
  "ax_mg,ay_mg,az_mg,"
  "gx_mrad_s,gy_mrad_s,gz_mrad_s,"
  "mx_mgauss,my_mgauss,mz_mgauss,"
  "press_hPa,temp_cC,"
  "geiger1,geiger2,"
  "lora_temp_cC,lora_volt_mV,"
  "lora_retries,lora_drops,storage_fails";

// Geiger ISRs — called from RISING-edge attachInterrupt() in App::begin().
void IRAM_ATTR geigerCh1Isr() { geigerCh1Pulses++; }
void IRAM_ATTR geigerCh2Isr() { geigerCh2Pulses++; }

static App app;

void setup() { app.begin(); }
void loop()  { app.tick();  }
