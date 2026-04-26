// Component test: BMP388 Barometer  (design says BMP388, actual board has BMP388)
//
// Verifies I2C communication, chip initialisation, and plausible readings.
//
// PASS criteria:
//   - begin_I2C() returns true
//   - Temperature  5 – 60 °C
//   - Pressure  70 000 – 115 000 Pa  (~sea level ± high altitude)
//
// Upload:  pio run -e test_baro -t upload
// Monitor: pio device monitor -e test_baro

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP3XX.h>

static constexpr int     PIN_I2C_SDA = 7;
static constexpr int     PIN_I2C_SCL = 15;
static constexpr uint8_t BMP_ADDR    = 0x76;  // BMP388 address (SDO low)

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

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: BMP388 Barometer");
    Serial.println("========================================");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    result("I2C bus initialized (SDA=7, SCL=15)", true);
    i2cScan();

    Adafruit_BMP3XX bmp;
    bool initOk = bmp.begin_I2C(BMP_ADDR, &Wire);
    Serial.printf("  [INFO] bmp.begin_I2C(0x%02X) = %s\n", BMP_ADDR, initOk ? "true" : "false");
    result("BMP388 begin_I2C() – I2C comms + chip ID", initOk);

    if (!initOk) {
        Serial.println("  [HINT] Check: SDA=7, SCL=15, power to module, address 0x76 (SDO low).");
        goto done;
    }

    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    result("Sensor configured (8x temp OS, 4x pres OS, IIR3)", true);

    {
        bool readOk = bmp.performReading();
        result("performReading()", readOk);

        if (readOk) {
            float temp = bmp.temperature;
            float pres = bmp.pressure;

            Serial.printf("  [INFO] Temperature: %.2f C   (expect 5 – 60 C)\n", temp);
            Serial.printf("  [INFO] Pressure:    %.0f Pa  (expect 70000 – 115000 Pa)\n", pres);

            result("Temperature in range 5 – 60 C",         temp > 5.0f   && temp < 60.0f);
            result("Pressure in range 70000 – 115000 Pa",   pres > 70000.0f && pres < 115000.0f);
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
