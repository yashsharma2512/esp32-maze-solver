// APDS-9960 proximity test derived from MAZESOLVER/header.h and proximity.ino.
// ESP32-WROOM-32 Dev Module + TCA9548A channel 0. No motors are controlled.
#include <Arduino.h>
#include <Wire.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr uint8_t TCA_ADDRESS = 0x70;
constexpr uint8_t APDS9960_ADDRESS = 0x39;
constexpr uint8_t APDS_CHANNEL = 0;

constexpr uint8_t APDS9960_REG_ENABLE = 0x80;
constexpr uint8_t APDS9960_REG_PPULSE = 0x8E;
constexpr uint8_t APDS9960_REG_ID = 0x92;
constexpr uint8_t APDS9960_REG_STATUS = 0x93;
constexpr uint8_t APDS9960_REG_PDATA = 0x9C;
constexpr uint8_t APDS9960_REG_CONFIG2 = 0x90;
constexpr uint8_t APDS9960_REG_CONTROL = 0x8F;

constexpr float alpha = 0.65f;  // Same smoothing factor as config_lolins2.h.
float filtered = 0.0f;
bool haveReading = false;

bool TCA9548A(uint8_t bus) {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(uint8_t(1U << bus));
  return Wire.endTransmission() == 0;
}

bool devicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// STATUS.PGSAT stays set until PICLEAR or proximity is disabled.
bool clearProximityFlag() {
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(uint8_t(0xE5));
  return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(APDS9960_ADDRESS, uint8_t(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool initAPDS(uint8_t channel) {
  if (!TCA9548A(channel)) return false;
  delay(20);
  if (!devicePresent(APDS9960_ADDRESS)) return false;

  // Use the original gain and LED boost values. Set controls before PEN.
  bool ok = writeRegister(APDS9960_REG_ENABLE, 0x00);
  ok = writeRegister(APDS9960_REG_CONFIG2, 0x31) && ok;
  ok = writeRegister(APDS9960_REG_CONTROL, 0x49) && ok;
  // Eight 8-us IR pulses. The original leaves PPULSE at one pulse.
  ok = writeRegister(APDS9960_REG_PPULSE, 0x87) && ok;
  ok = writeRegister(APDS9960_REG_ENABLE, 0x01) && ok; // PON
  delay(10);
  ok = writeRegister(APDS9960_REG_ENABLE, 0x05) && ok; // PON + PEN
  delay(50);
  return ok;
}

bool readProximity(uint8_t channel, uint8_t &proximity) {
  if (!TCA9548A(channel)) return false;
  // PDATA is one byte. The original sketch requests two, then treats the
  // neighboring register as a high byte; this test reads PDATA alone.
  return readRegister(APDS9960_REG_PDATA, proximity);
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);  // Same I2C speed as the original beginWire().
  Wire.setTimeOut(50);

  Serial.println("\nESP32 + TCA9548A + APDS-9960 proximity test");
  Serial.printf("SDA=%d SCL=%d, TCA=0x%02X, channel=%u, APDS=0x%02X\n",
                SDA_PIN, SCL_PIN, TCA_ADDRESS, APDS_CHANNEL, APDS9960_ADDRESS);
  Serial.println("Keep motors off. Move a hand toward and away from the sensor.");

  if (!devicePresent(TCA_ADDRESS)) {
    Serial.println("ERROR: TCA9548A not found. Check SDA, SCL, power, GND, A0-A2.");
    return;
  }
  if (!initAPDS(APDS_CHANNEL)) {
    Serial.println("ERROR: APDS-9960 on channel 0 did not initialize.");
    return;
  }
  uint8_t id, enable, pulse, control, config2;
  if (!readRegister(APDS9960_REG_ID, id) ||
      !readRegister(APDS9960_REG_ENABLE, enable) ||
      !readRegister(APDS9960_REG_PPULSE, pulse) ||
      !readRegister(APDS9960_REG_CONTROL, control) ||
      !readRegister(APDS9960_REG_CONFIG2, config2)) {
    Serial.println("ERROR: APDS-9960 register readback failed.");
    return;
  }
  Serial.printf("APDS ID=0x%02X ENABLE=0x%02X PPULSE=0x%02X CONTROL=0x%02X CONFIG2=0x%02X\n",
                id, enable, pulse, control, config2);
  if (id != 0xAB) Serial.println("WARNING: unexpected APDS-9960 ID (expected 0xAB).");
  if (enable != 0x05 || pulse != 0x87 || control != 0x49 || config2 != 0x31)
    Serial.println("WARNING: configuration did not read back as written.");
  Serial.println("Raw range: 0..255. valid=1 means a proximity conversion completed.");
}

void loop() {
  static uint32_t previousMs = 0;
  uint32_t now = millis();
  if (now - previousMs < 100) return;
  previousMs = now;

  uint8_t raw, status;
  if (!TCA9548A(APDS_CHANNEL) ||
      !readRegister(APDS9960_REG_STATUS, status) ||
      !readProximity(APDS_CHANNEL, raw)) {
    Serial.println("READ ERROR: TCA channel 0 or APDS-9960 did not respond.");
    return;
  }
  bool valid = (status & 0x02) != 0;
  bool saturated = (status & 0x40) != 0;
  if (saturated && !clearProximityFlag()) {
    Serial.println("READ ERROR: could not clear APDS saturation flag.");
    return;
  }
  if (!valid) {
    Serial.printf("CH0 INVALID status=0x%02X raw=%3u (no completed proximity sample)\n",
                  status, raw);
    return;
  }
  filtered = haveReading ? alpha * filtered + (1.0f - alpha) * raw : raw;
  haveReading = true;
  Serial.printf("CH0 raw=%3u filtered=%6.1f valid=1 saturated=%u\n",
                raw, filtered, saturated);
}
