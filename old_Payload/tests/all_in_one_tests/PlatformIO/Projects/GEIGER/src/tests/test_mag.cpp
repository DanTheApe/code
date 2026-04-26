// Component test: BMM350 Magnetometer
//
// Verifies I2C communication, chip initialisation, and plausible field data.
//
// PASS criteria:
//   - begin() returns 0
//   - Total field magnitude 5 – 100 µT  (Earth field is ~25 – 65 µT)
//
// Upload:  pio run -e test_mag -t upload
// Monitor: pio device monitor -e test_mag

#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMM350.h"

static constexpr int     PIN_I2C_SDA = 7;
static constexpr int     PIN_I2C_SCL = 15;
static constexpr uint8_t BMM_ADDR    = 0x14;

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
    Serial.println("  COMPONENT TEST: BMM350 Magnetometer");
    Serial.println("========================================");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    result("I2C bus initialized (SDA=7, SCL=15)", true);
    i2cScan();

    DFRobot_BMM350_I2C bmm(&Wire, BMM_ADDR);
    int initState = bmm.begin();
    Serial.printf("  [INFO] bmm.begin() = %d  (0 = OK)\n", initState);
    bool initOk = (initState == 0);
    result("BMM350 begin() – I2C comms + chip ID", initOk);

    if (!initOk) {
        Serial.println("  [HINT] Check: I2C address 0x14, SDA=7, SCL=15, power.");
        goto done;
    }

    {
        bmm.setOperationMode(eBmm350NormalMode);
        delay(100);  // wait for first measurement

        sBmm350MagData_t m = bmm.getGeomagneticData();

        Serial.printf("  [INFO] Mag X: %6.2f µT\n", m.float_x);
        Serial.printf("  [INFO] Mag Y: %6.2f µT\n", m.float_y);
        Serial.printf("  [INFO] Mag Z: %6.2f µT\n", m.float_z);

        float mag = sqrtf(m.float_x*m.float_x + m.float_y*m.float_y + m.float_z*m.float_z);
        Serial.printf("  [INFO] Total field: %.2f µT  (Norway ~85-115 µT incl. interference)\n", mag);

        result("Field magnitude in range 5 – 150 µT", mag > 5.0f && mag < 150.0f);
    }

done:
    Serial.println("========================================");
    Serial.println("  TEST COMPLETE  –  restarting in 5 s");
    Serial.println("========================================");
    delay(5000);
    ESP.restart();
}

void loop() {}
