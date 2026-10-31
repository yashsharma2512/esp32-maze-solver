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

  // Same register settings as the original initAPDS() in proximity.ino.
  bool ok = writeRegister(APDS9960_REG_ENABLE,
                          (0 << 6) | (1 << 5) | (0 << 4) |
                          (0 << 3) | (1 << 2) | (0 << 1) | 1);
  ok = writeRegister(APDS9960_REG_CONFIG2,
                     (0 << 7) | (0 << 6) | (0x03 << 4) | 1) && ok;
  ok = writeRegister(APDS9960_REG_CONTROL,
                     (0x01 << 6) | (0x02 << 2) | 0x01) && ok;
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
  Serial.println("APDS-9960 initialized. Raw range: 0..255.");
}

void loop() {
  static uint32_t previousMs = 0;
  uint32_t now = millis();
  if (now - previousMs < 100) return;
  previousMs = now;

  uint8_t raw;
  if (!readProximity(APDS_CHANNEL, raw)) {
    Serial.println("READ ERROR: TCA channel 0 or APDS-9960 did not respond.");
    return;
  }
  filtered = haveReading ? alpha * filtered + (1.0f - alpha) * raw : raw;
  haveReading = true;
  Serial.printf("CH0 raw=%3u filtered=%6.1f\n", raw, filtered);
}
