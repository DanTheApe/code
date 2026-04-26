// Component test: SD Card
//
// Verifies SPI wiring, card mount, write, read-back, and verify.
//
// PASS criteria:
//   - SD.begin() returns true
//   - Card type is not CARD_NONE
//   - Write → read-back matches expected string
//
// Upload:  pio run -e test_sdcard -t upload
// Monitor: pio device monitor -e test_sdcard

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

static constexpr int PIN_SPI_MOSI = 4;
static constexpr int PIN_SPI_SCK  = 5;
static constexpr int PIN_SPI_MISO = 6;
static constexpr int SD_CS        = 21;

static const char* TEST_FILE = "/sdtest.txt";
static const char* TEST_DATA = "SDCARD_COMPONENT_TEST_OK";

static void result(const char* label, bool pass) {
    Serial.printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: SD Card");
    Serial.println("========================================");

    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    result("SPI bus initialized (SCK=5, MISO=6, MOSI=4)", true);

    bool mounted = SD.begin(SD_CS, SPI, 8000000);
    Serial.printf("  [INFO] SD.begin() = %s\n", mounted ? "true" : "false");
    result("SD mount (CS=21)", mounted);

    if (!mounted) {
        Serial.println("  [HINT] Check: card inserted, CS=21, SPI pins 4/5/6, 3.3 V supply.");
        goto done;
    }

    {
        uint8_t cardType = SD.cardType();
        const char* typeName =
            cardType == CARD_MMC  ? "MMC"  :
            cardType == CARD_SD   ? "SD"   :
            cardType == CARD_SDHC ? "SDHC" : "UNKNOWN";
        Serial.printf("  [INFO] Card type: %s\n", typeName);
        Serial.printf("  [INFO] Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
        result("Card type detected", cardType != CARD_NONE);

        // Clean up from any previous aborted run
        SD.remove(TEST_FILE);

        // Write
        File fw = SD.open(TEST_FILE, FILE_WRITE);
        bool writeOk = false;
        if (fw) {
            writeOk = (fw.print(TEST_DATA) > 0);
            fw.close();
        }
        result("Write test file", writeOk);

        if (writeOk) {
            // Read back
            File fr = SD.open(TEST_FILE, FILE_READ);
            bool verifyOk = false;
            if (fr) {
                String content = fr.readString();
                fr.close();
                verifyOk = (content == String(TEST_DATA));
                Serial.printf("  [INFO] Read back: \"%s\"\n", content.c_str());
            }
            result("Read back and verify", verifyOk);

            SD.remove(TEST_FILE);
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
