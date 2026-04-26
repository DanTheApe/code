// Component test: SX1280 LoRa Radio
//
// Verifies SPI wiring, chip presence, configuration, and a live TX.
//
// PASS criteria:
//   - radio.begin() returns RADIOLIB_ERR_NONE  (SPI comms + chip-ID check)
//   - All config calls succeed
//   - transmit() returns RADIOLIB_ERR_NONE
//
// Upload:  pio run -e test_lora -t upload
// Monitor: pio device monitor -e test_lora

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// Pin map – must match Config.hpp
static constexpr int PIN_SPI_MOSI  = 4;
static constexpr int PIN_SPI_SCK   = 5;
static constexpr int PIN_SPI_MISO  = 6;
static constexpr int LORA_NSS      = 9;   // IF13 CS_rf
static constexpr int LORA_POWER_EN = 10;  // IF14 power enable
static constexpr int LORA_DIO1     = 11;  // IF15 CXT
static constexpr int LORA_DIO2     = 12;  // IF16 CrX
static constexpr int LORA_BUSY     = 13;  // IF17 rf_busy
static constexpr int LORA_RST      = 14;  // IF18 rf_rst

static void result(const char* label, bool pass) {
    Serial.printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: SX1280 LoRa Radio");
    Serial.println("========================================");

    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    result("SPI bus initialized (SCK=5, MISO=6, MOSI=4)", true);

    // Enable power to LoRa module
    pinMode(LORA_POWER_EN, OUTPUT); digitalWrite(LORA_POWER_EN, HIGH);
    delay(10);
    result("LoRa power enabled (GPIO10/IF14)", true);

    // Manual reset pulse before handing control to RadioLib
    pinMode(LORA_NSS,  OUTPUT); digitalWrite(LORA_NSS,  HIGH);
    pinMode(LORA_RST,  OUTPUT); digitalWrite(LORA_RST,  LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(20);

    // Check BUSY: SX1280 must go LOW within ~3 ms of reset release
    pinMode(LORA_BUSY, INPUT);
    uint32_t t0 = millis();
    while (digitalRead(LORA_BUSY) == HIGH && millis() - t0 < 100) {}
    bool busyOk = (digitalRead(LORA_BUSY) == LOW);
    Serial.printf("  [INFO] BUSY after reset = %s (waited %lu ms)\n",
                  busyOk ? "LOW (OK)" : "HIGH (stuck!)", millis() - t0);
    result("BUSY pin LOW after reset", busyOk);

    // Raw SPI loopback: read one byte with NSS asserted – should not be 0x00 or 0xFF
    {
        SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
        digitalWrite(LORA_NSS, LOW);
        delayMicroseconds(2);
        uint8_t b = SPI.transfer(0xC0);  // GetStatus opcode
        SPI.transfer(0x00);
        uint8_t status = SPI.transfer(0x00);
        digitalWrite(LORA_NSS, HIGH);
        SPI.endTransaction();
        Serial.printf("  [INFO] Raw SPI GetStatus response: 0x%02X (0x00/0xFF = no comms)\n", status);
    }

    Module* mod = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                             SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0));
    SX1280 radio(mod);

    // begin() does SPI transactions and verifies the chip ID
    int state = radio.begin();
    Serial.printf("  [INFO] radio.begin() = %d  (0 = OK)\n", state);
    bool initOk = (state == RADIOLIB_ERR_NONE);
    result("radio.begin() – SPI comms + chip ID", initOk);

    if (!initOk) {
        Serial.println("  [HINT] Check: NSS=9(IF13), DIO1=11(IF15), RST=14(IF18), BUSY=13(IF17), PWR_EN=10(IF14).");
        goto done;
    }

    {
        bool cfgOk = true;
        int s;

        s = radio.setFrequency(2400.0f);
        if (s != RADIOLIB_ERR_NONE) { Serial.printf("  [FAIL] setFrequency: %d\n", s); cfgOk = false; }

        s = radio.setBandwidth(812.5f);
        if (s != RADIOLIB_ERR_NONE) { Serial.printf("  [FAIL] setBandwidth: %d\n", s); cfgOk = false; }

        s = radio.setSpreadingFactor(7);
        if (s != RADIOLIB_ERR_NONE) { Serial.printf("  [FAIL] setSpreadingFactor: %d\n", s); cfgOk = false; }

        s = radio.setCodingRate(5);
        if (s != RADIOLIB_ERR_NONE) { Serial.printf("  [FAIL] setCodingRate: %d\n", s); cfgOk = false; }

        s = radio.setOutputPower(13);
        if (s != RADIOLIB_ERR_NONE) { Serial.printf("  [FAIL] setOutputPower: %d\n", s); cfgOk = false; }

        result("Radio configuration (2400 MHz / 812.5 kHz BW / SF7 / CR4/5 / 13 dBm)", cfgOk);

        if (cfgOk) {
            // Check DIO1 state before TX – should be LOW when idle
            pinMode(LORA_DIO1, INPUT);
            Serial.printf("  [INFO] DIO1 (GPIO11) before TX = %s\n",
                          digitalRead(LORA_DIO1) ? "HIGH" : "LOW");

            // Start non-blocking TX, poll for completion manually
            String msg = "LORA_TEST";
            int txState = radio.startTransmit(msg);
            Serial.printf("  [INFO] startTransmit() = %d  (0 = OK)\n", txState);

            if (txState == RADIOLIB_ERR_NONE) {
                uint32_t tTx = millis();
                while (digitalRead(LORA_BUSY) == HIGH && millis() - tTx < 2000) {}
                bool txDone = (digitalRead(LORA_BUSY) == LOW);
                Serial.printf("  [INFO] TX BUSY cleared = %s (waited %lu ms)\n",
                              txDone ? "YES" : "NO (timeout)", millis() - tTx);
                Serial.printf("  [INFO] DIO1 after TX    = %s\n",
                              digitalRead(LORA_DIO1) ? "HIGH (IRQ)" : "LOW");
                radio.finishTransmit();
                result("Transmit test packet", txDone);
            } else {
                result("Transmit test packet", false);
            }
        }
    }

done:
    Serial.println("========================================");
    Serial.println("  TEST COMPLETE  –  restarting in 5 s");
    Serial.println("========================================");
    delay(5000);
    ESP.restart();
}

void loop() {}
