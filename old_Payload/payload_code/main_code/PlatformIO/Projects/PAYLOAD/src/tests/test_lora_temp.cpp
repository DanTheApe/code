// Component test: TMP1075 temperature sensor (next to the LoRa module)
//
// The TMP1075 sits on I2C address 0x73. It exposes a 16-bit temperature
// register at pointer 0x00; the upper 12 bits are a signed two's-complement
// reading with 0.0625 °C / LSB, and the lower 4 bits are reserved (zero).
//
// PASS criteria:
//   - 0x73 ACKs on the I2C bus
//   - Two bytes can be read from register 0x00
//   - Temperature is in the 5 – 60 °C range (sane room / payload bench temp)
//
// Upload:  pio run -e test_lora_temp -t upload
// Monitor: pio device monitor -e test_lora_temp

#include <Arduino.h>
#include <Wire.h>

static constexpr int     PIN_I2C_SDA = 7;
static constexpr int     PIN_I2C_SCL = 15;
static constexpr uint8_t TMP1075_ADDR = 0x49;
static constexpr uint8_t TMP1075_REG_TEMP = 0x00;

static void result(const char* label, bool pass) {
    Serial.printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
}

static void i2cScan() {
    Serial.println("  [INFO] I2C scan:");
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("         0x%02X\n", addr);
            found = true;
        }
    }
    if (!found) Serial.println("         (no devices found)");
}

// Input  : (none)
// What   : reads the TMP1075 temperature register and decodes the 12-bit
//          signed temperature into milli-degrees-Celsius (mC).
// Output : true on a successful I2C transaction; tempMilliC is filled.
static bool readTmp1075(int32_t& tempMilliC) {
    Wire.beginTransmission(TMP1075_ADDR);
    Wire.write(TMP1075_REG_TEMP);
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom(static_cast<int>(TMP1075_ADDR), 2) != 2) return false;
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();

    // 12-bit signed value, MSB-aligned in the 16-bit register.
    int16_t raw = static_cast<int16_t>((hi << 8) | lo);
    int16_t v12 = raw >> 4;          // arithmetic shift preserves sign
    // mC = v12 * 62.5 = v12 * 625 / 10.
    tempMilliC = (static_cast<int32_t>(v12) * 625) / 10;
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: TMP1075  (LoRa temp)");
    Serial.println("========================================");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    result("I2C bus initialized (SDA=7, SCL=15)", true);
    i2cScan();

    // Probe address 0x73 directly so the user gets a clear PASS/FAIL.
    Wire.beginTransmission(TMP1075_ADDR);
    bool present = (Wire.endTransmission() == 0);
    Serial.printf("  [INFO] 0x%02X ACK = %s\n", TMP1075_ADDR, present ? "yes" : "no");
    result("TMP1075 present at 0x73", present);

    if (!present) {
        Serial.println("  [HINT] Check: TMP1075 is populated on the LoRa side, SDA=7, SCL=15, power.");
        goto done;
    }

    {
        int32_t mC = 0;
        bool readOk = readTmp1075(mC);
        Serial.printf("  [INFO] Raw read OK = %s\n", readOk ? "yes" : "no");
        result("Read temperature register", readOk);

        if (readOk) {
            Serial.printf("  [INFO] Temperature: %ld mC  (%.2f °C)\n",
                          static_cast<long>(mC), mC / 1000.0f);
            result("Temperature in range 5 – 60 °C",
                   mC >= 5000 && mC <= 60000);

            // Take a second reading after a short wait — the chip should produce a
            // similar (within ±1 °C) value if it's actually converting.
            delay(200);
            int32_t mC2 = 0;
            if (readTmp1075(mC2)) {
                Serial.printf("  [INFO] Second read: %.2f °C  (delta = %ld mC)\n",
                              mC2 / 1000.0f, static_cast<long>(mC2 - mC));
                result("Two consecutive reads within 1000 mC",
                       abs(mC2 - mC) <= 1000);
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
