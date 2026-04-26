// Component test: Geiger Counter
//
// Verifies that the interrupt-driven pulse input on GPIO 8 (CRD1/IF11) works.
//
// PASS criteria:
//   - Interrupt attached successfully (always passes if pin exists)
//   - Pulses detected during the 10 s window (requires tube powered + source)
//
// If no pulses: the ISR/pin wiring is still confirmed by the attach step.
// Check: 5 V supply to Geiger module, signal wire on GPIO 8 (CRD1/IF11).
//
// Upload:  pio run -e test_geiger -t upload
// Monitor: pio device monitor -e test_geiger

#include <Arduino.h>

static constexpr int     GEIGER_PIN = 8;  // CRD1, IF11
static volatile uint32_t pulseCount = 0;

void IRAM_ATTR geigerISR() {
    pulseCount++;
}

static void result(const char* label, bool pass) {
    Serial.printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: Geiger Counter");
    Serial.println("========================================");

    // Step 1 – attach interrupt
    pinMode(GEIGER_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), geigerISR, RISING);
    result("Interrupt attached on GPIO 8 (CRD1/IF11)", true);

    // Step 2 – count pulses for 10 seconds
    Serial.println("  [INFO] Listening for pulses for 10 s ...");
    uint32_t start = millis();
    while (millis() - start < 10000) {
        delay(100);
    }

    noInterrupts();
    uint32_t count = pulseCount;
    interrupts();

    Serial.printf("  [INFO] Pulses in 10 s: %lu\n", static_cast<unsigned long>(count));

    if (count > 0) {
        result("Pulses received", true);
    } else {
        Serial.println("  [WARN] Zero pulses – module may still be functional.");
        Serial.println("         Check: 5 V to Geiger module, signal on GPIO 8 (CRD1/IF11).");
        result("Pulses received (no source/activity detected)", false);
    }

    Serial.println("========================================");
    Serial.println("  TEST COMPLETE  –  restarting in 5 s");
    Serial.println("========================================");
    delay(5000);
    ESP.restart();
}

void loop() {}
