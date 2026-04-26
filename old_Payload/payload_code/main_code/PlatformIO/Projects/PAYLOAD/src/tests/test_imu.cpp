// Component test: ICM-45686 IMU (accelerometer + gyroscope)
//
// Verifies I2C communication, chip initialisation, and plausible sensor data.
//
// PASS criteria:
//   - IMU found on I2C bus
//   - begin() returns 0
//   - Accel magnitude 0.5 – 2.0 g  (gravity shows on at least one axis)
//   - Gyro magnitude < 50 dps when the board is held still
//
// Upload:  pio run -e test_imu -t upload
// Monitor: pio device monitor -e test_imu

#include <Arduino.h>
#include <SPI.h>
#include <ICM45686.h>

static constexpr int PIN_SPI_SCK  = 5;
static constexpr int PIN_SPI_MISO = 6;
static constexpr int PIN_SPI_MOSI = 4;
static constexpr int IMU_CS       = 16;  // IF8

static void result(const char* label, bool pass) {
    Serial.printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
}


void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: ICM-45686 IMU");
    Serial.println("========================================");

    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    result("SPI bus initialized (SCK=5, MISO=6, MOSI=4, CS=16/IF8)", true);

    ICM456xx imu(SPI, IMU_CS);
    int initState = imu.begin();
    Serial.printf("  [INFO] imu.begin() = %d  (0 = OK)\n", initState);
    bool initOk = (initState == 0);
    result("IMU begin() – I2C comms + chip ID", initOk);

    if (!initOk) {
        Serial.println("  [HINT] Check: SPI wiring SCK=5, MISO=6, MOSI=4, CS=16(IF8), power.");
        goto done;
    }

    imu.startAccel(100, 16);
    imu.startGyro(100, 2000);
    result("Accel + Gyro started at 100 Hz", true);

    delay(200);  // let first sample arrive

    {
        inv_imu_sensor_data_t data;
        int readState = imu.getDataFromRegisters(data);
        Serial.printf("  [INFO] getDataFromRegisters() = %d  (0 = OK)\n", readState);
        bool readOk = (readState == 0);
        result("Read sensor registers", readOk);

        if (readOk) {
            float ax = data.accel_data[0];
            float ay = data.accel_data[1];
            float az = data.accel_data[2];
            float gx = data.gyro_data[0];
            float gy = data.gyro_data[1];
            float gz = data.gyro_data[2];

            Serial.printf("  [INFO] Accel  X=%.3f g   Y=%.3f g   Z=%.3f g\n", ax, ay, az);
            Serial.printf("  [INFO] Gyro   X=%.2f dps  Y=%.2f dps  Z=%.2f dps\n", gx, gy, gz);

            float aMag = sqrtf(ax*ax + ay*ay + az*az);
            float gMag = sqrtf(gx*gx + gy*gy + gz*gz);
            Serial.printf("  [INFO] Accel magnitude: %.3f g  (expect ~1.0)\n", aMag);
            Serial.printf("  [INFO] Gyro  magnitude: %.2f dps (expect ~0 if still)\n", gMag);

            result("Accel magnitude in range 0.5 – 2.0 g", aMag > 0.5f && aMag < 2.0f);
            result("Gyro magnitude at rest < 50 dps",      gMag < 50.0f);
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
