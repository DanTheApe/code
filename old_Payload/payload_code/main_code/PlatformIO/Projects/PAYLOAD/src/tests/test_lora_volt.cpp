// Component test: DAC43401 (LoRa TX-power voltage setter)
//
// The DAC43401 is a single-channel 8-bit DAC at I2C address 0x72.  It
// exposes a 16-bit DEVICE_ID register (read-only, addr 0x01) and a
// 16-bit DAC_DATA register (addr 0x21) whose top 8 bits hold the output
// code.  Output voltage = code * VREF / 255 (VREF ≈ VDD ≈ 3.3 V on this
// board, unless the internal reference is enabled in GENERAL_CONFIG).
//
// Without an external multimeter we cannot verify the actual analogue
// output, but we *can* prove I2C comms work and the chip accepts writes.
//
// PASS criteria:
//   - 0x72 ACKs on the I2C bus
//   - DEVICE_ID register can be read (two bytes, non-zero)
//   - Mid-scale, full-scale, and zero-scale writes all ACK
//
// Upload:  pio run -e test_lora_volt -t upload
// Monitor: pio device monitor -e test_lora_volt

#include <Arduino.h>
#include <Wire.h>

static constexpr int     PIN_I2C_SDA   = 7;
static constexpr int     PIN_I2C_SCL   = 15;
static constexpr int     PIN_LORA_PWR  = 10;   // gates LoRa-side power rail.

// The breakout PDF claimed the DAC43401 sits at 0x72, but the real strapping
// places it somewhere else entirely.  We probe the whole bus and rely on a
// behavioural check (DAC_DATA round-trips two distinct values) to identify
// the chip — a TMP1075 in the same address range will fail because its
// register pointer wraps and its config register masks reserved bits.
static constexpr uint8_t DAC_REG_STATUS        = 0x01;
static constexpr uint8_t DAC_REG_GENERAL_CFG   = 0x09;
static constexpr uint8_t DAC_REG_DAC_DATA      = 0x21;
static constexpr uint8_t DAC_REG_DEVICE_UNLOCK = 0x36;
static constexpr uint16_t DAC_UNLOCK_MAGIC     = 0x5000;

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

// Input  : addr - I2C address; reg - DAC register; value - 16-bit data.
// What   : writes [reg] [value_hi] [value_lo] in one I2C transaction.
// Output : true if the device ACKed the write, false otherwise.
static bool dacWrite16(uint8_t addr, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(static_cast<uint8_t>(value >> 8));
    Wire.write(static_cast<uint8_t>(value & 0xFF));
    return (Wire.endTransmission() == 0);
}

// Input  : addr - I2C address; reg - DAC register; out - destination uint16.
// What   : performs a "write reg pointer, repeated start, read 2 bytes"
//          sequence and stores the resulting 16-bit big-endian value.
// Output : true if all phases succeeded, false otherwise.
static bool dacRead16(uint8_t addr, uint8_t reg, uint16_t& out) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(addr), 2) != 2) return false;
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    out = (static_cast<uint16_t>(hi) << 8) | lo;
    return true;
}

// Input  : addr - I2C address; code - 8-bit DAC output code (0..255).
// What   : packs the code into the top 8 bits of DAC_DATA and writes it.
// Output : true on a successful write.
static bool dacSetCode(uint8_t addr, uint8_t code) {
    return dacWrite16(addr, DAC_REG_DAC_DATA, static_cast<uint16_t>(code) << 8);
}

// Input  : addr - candidate I2C address.
// What   : runs the DAC43401 wake-up sequence (unlock + clear power-down),
//          then writes two distinct test patterns to DAC_DATA and confirms
//          each one round-trips.  Status / config register reads are
//          printed so a partial failure (e.g. ACK but no round-trip) shows
//          where the chip stopped cooperating.
// Output : true if both patterns round-trip exactly, false otherwise.
static bool looksLikeDac(uint8_t addr) {
    uint16_t status = 0;
    bool sOk = dacRead16(addr, DAC_REG_STATUS, status);
    Serial.printf("                  STATUS=0x%04X (read=%s)\n",
                  status, sOk ? "yes" : "no");

    // Send the magic unlock value (no-op if device wasn't locked).
    bool uOk = dacWrite16(addr, DAC_REG_DEVICE_UNLOCK, DAC_UNLOCK_MAGIC);
    Serial.printf("                  unlock write ACK=%s\n", uOk ? "yes" : "no");
    delay(2);

    // Clear GENERAL_CONFIG so the DAC isn't in power-down.
    bool gOk = dacWrite16(addr, DAC_REG_GENERAL_CFG, 0x0000);
    Serial.printf("                  general-cfg write ACK=%s\n", gOk ? "yes" : "no");
    delay(2);

    constexpr uint16_t patterns[] = {0xA500, 0x5A00};
    for (uint16_t p : patterns) {
        if (!dacWrite16(addr, DAC_REG_DAC_DATA, p)) return false;
        delay(2);
        uint16_t readBack = 0;
        if (!dacRead16(addr, DAC_REG_DAC_DATA, readBack)) return false;
        Serial.printf("                  wrote 0x%04X, read 0x%04X\n", p, readBack);
        if (readBack != p) return false;
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  COMPONENT TEST: DAC43401  (LoRa volt)");
    Serial.println("========================================");

    // Enable the LoRa-side power rail; the DAC43401 may sit on it.
    pinMode(PIN_LORA_PWR, OUTPUT);
    digitalWrite(PIN_LORA_PWR, HIGH);
    delay(20);
    result("LoRa power enable HIGH (GPIO10)", true);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    result("I2C bus initialized (SDA=7, SCL=15)", true);
    i2cScan();

    // Explicitly probe the addresses the original breakout PDF claimed for
    // the DAC and the LoRa-temp sensor (0x72 / 0x73).  These almost never
    // show up in the scan above, but ruling them out by name makes the
    // diagnostic unambiguous.
    Serial.println("  [INFO] PDF-claimed addresses:");
    for (uint8_t addr : {0x72, 0x73}) {
        Wire.beginTransmission(addr);
        bool ack = (Wire.endTransmission() == 0);
        Serial.printf("         0x%02X  ACK=%s\n", addr, ack ? "yes" : "no");
        if (ack) {
            bool isDac = looksLikeDac(addr);
            Serial.printf("                  looksLikeDac=%s\n", isDac ? "yes" : "no");
            if (isDac) {
                Serial.printf("  [INFO] DAC43401 *does* live at 0x%02X — update Config.hpp.\n", addr);
            }
        }
    }


    // Probe every address that ACKed and behavioural-check it.  Only an
    // actual DAC43401 will round-trip both test patterns to DAC_DATA.
    Serial.println("  [INFO] behavioural probe of every responding address:");
    uint8_t dacAddr = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) continue;
        bool isDac = looksLikeDac(addr);
        Serial.printf("         0x%02X  looksLikeDac=%s\n",
                      addr, isDac ? "yes" : "no");
        if (isDac && dacAddr == 0) dacAddr = addr;
    }

    result("DAC43401 found on the bus", dacAddr != 0);
    if (dacAddr == 0) {
        Serial.println("  [HINT] If LoRa-power-enable (GPIO10) is not HIGH, the DAC has no power.");
        Serial.println("  [HINT] If the DAC is missing on this board variant, this is expected.");
        goto done;
    }
    Serial.printf("  [INFO] using DAC at 0x%02X\n", dacAddr);

    {
        uint16_t status = 0;
        bool stOk = dacRead16(dacAddr, DAC_REG_STATUS, status);
        Serial.printf("  [INFO] STATUS register = 0x%04X (read=%s)\n",
                      status, stOk ? "yes" : "no");
        result("Read STATUS register",  stOk);

        // Mid-scale write (~ VREF / 2 ≈ 1.65 V on this board).
        bool midOk = dacSetCode(dacAddr, 0x80);
        Serial.printf("  [INFO] Mid-scale write   (code 0x80, ~1.65 V) ACK=%s\n",
                      midOk ? "yes" : "no");
        result("Write mid-scale (0x80)",   midOk);
        delay(20);

        // Full-scale write (~ VREF ≈ 3.3 V).
        bool fullOk = dacSetCode(dacAddr, 0xFF);
        Serial.printf("  [INFO] Full-scale write  (code 0xFF, ~3.3 V)  ACK=%s\n",
                      fullOk ? "yes" : "no");
        result("Write full-scale (0xFF)",  fullOk);
        delay(20);

        // Zero-scale write (0 V).
        bool zeroOk = dacSetCode(dacAddr, 0x00);
        Serial.printf("  [INFO] Zero-scale write  (code 0x00, 0 V)     ACK=%s\n",
                      zeroOk ? "yes" : "no");
        result("Write zero-scale (0x00)",  zeroOk);
        delay(20);

        // Read back DAC_DATA — the chip should hold the last value we wrote.
        uint16_t readBack = 0;
        bool rbOk = dacRead16(dacAddr, DAC_REG_DAC_DATA, readBack);
        uint8_t lastCode = static_cast<uint8_t>(readBack >> 8);
        Serial.printf("  [INFO] DAC_DATA read back = 0x%04X (code=0x%02X, read=%s)\n",
                      readBack, lastCode, rbOk ? "yes" : "no");
        result("Read DAC_DATA register",   rbOk);
        result("Read-back matches last write (0x00)",
               rbOk && lastCode == 0x00);

        // Leave the DAC at a safe mid-scale so the LoRa PA sees a known voltage.
        dacSetCode(dacAddr, 0x80);
        Serial.printf("  [INFO] DAC at 0x%02X left at mid-scale (~1.65 V) on exit.\n", dacAddr);
    }

done:
    Serial.println("========================================");
    Serial.println("  TEST COMPLETE  –  restarting in 5 s");
    Serial.println("========================================");
    delay(5000);
    ESP.restart();
}

void loop() {}
