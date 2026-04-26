// Component test: LittleFS onboard flash
//
// Verifies LittleFS mount, write, read-back, and verify on the ESP32-S3
// internal flash partition.
//
// PASS criteria:
//   - LittleFS.begin() returns true
//   - Write → read-back matches expected string
//
// Upload:  pio run -e test_flash -t upload
// Monitor: pio device monitor -e test_flash

#include <Arduino.h>
#include <LittleFS.h>

static const char* TEST_FILE = "/flash_test.txt";
static const char* TEST_DATA = "FLASH_COMPONENT_TEST_OK";

static void result(const char* label, bool pass) {
    Serial.printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: LittleFS Flash");
    Serial.println("========================================");

    // true = format partition if it cannot be mounted
    bool mounted = LittleFS.begin(true);
    Serial.printf("  [INFO] LittleFS.begin() = %s\n", mounted ? "true" : "false");
    result("LittleFS mount", mounted);

    if (!mounted) {
        Serial.println("  [HINT] Flash partition may be missing from partitions.csv.");
        goto done;
    }

    Serial.printf("  [INFO] Total: %u bytes\n", static_cast<unsigned>(LittleFS.totalBytes()));
    Serial.printf("  [INFO] Used:  %u bytes\n", static_cast<unsigned>(LittleFS.usedBytes()));

    {
        // Clean up from any previous aborted run
        LittleFS.remove(TEST_FILE);

        // Write
        File fw = LittleFS.open(TEST_FILE, FILE_WRITE);
        bool writeOk = false;
        if (fw) {
            writeOk = (fw.print(TEST_DATA) > 0);
            fw.close();
        }
        result("Write test file", writeOk);

        if (writeOk) {
            // Read back
            File fr = LittleFS.open(TEST_FILE, FILE_READ);
            bool verifyOk = false;
            if (fr) {
                String content = fr.readString();
                fr.close();
                verifyOk = (content == String(TEST_DATA));
                Serial.printf("  [INFO] Read back: \"%s\"\n", content.c_str());
            }
            result("Read back and verify", verifyOk);

            LittleFS.remove(TEST_FILE);
            result("Cleanup test file", true);
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
