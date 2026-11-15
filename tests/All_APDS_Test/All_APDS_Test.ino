#include <Arduino.h>
#include <Wire.h>

// Matches firmware/MAZESOLVER/config/config_esp32_dev.h and proximity.ino.
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t MOTOR_STBY_PIN = 33;
constexpr uint8_t TCA_ADDRESS = 0x70;
constexpr uint8_t APDS_ADDRESS = 0x39;
constexpr uint8_t APDS_GAIN = 2;  // 1, 2, 4, or 8 times.
constexpr float FILTER_ALPHA = 0.65f;

constexpr uint8_t REG_ENABLE = 0x80;
constexpr uint8_t REG_PPULSE = 0x8E;
constexpr uint8_t REG_CONTROL = 0x8F;
constexpr uint8_t REG_CONFIG2 = 0x90;
constexpr uint8_t REG_ID = 0x92;
constexpr uint8_t REG_PDATA = 0x9C;

struct Sensor {
  const char *name;
  uint8_t channel;
  float filtered = 0;
  bool hasSample = false;
};

Sensor sensors[] = {{"LEFT", 0}, {"RIGHT", 1}, {"FRONT", 6}};

bool selectChannel(uint8_t channel) {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(uint8_t(1u << channel));
  return Wire.endTransmission() == 0;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(APDS_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(APDS_ADDRESS, uint8_t(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool initSensor(Sensor &sensor) {
  if (!selectChannel(sensor.channel)) return false;
  delay(20);
  const uint8_t gainCode = APDS_GAIN == 1 ? 0 : APDS_GAIN == 2 ? 1 :
                           APDS_GAIN == 4 ? 2 : 3;
  if (!writeRegister(REG_ENABLE, 0x00) ||
      !writeRegister(REG_CONFIG2, 0x31) ||
      !writeRegister(REG_CONTROL, uint8_t(0x41 | (gainCode << 2))) ||
      !writeRegister(REG_PPULSE, 0x87) ||
      !writeRegister(REG_ENABLE, 0x01)) return false;
  delay(10);
  if (!writeRegister(REG_ENABLE, 0x05)) return false;
  delay(50);
  uint8_t id = 0;
  if (!readRegister(REG_ID, id)) return false;
  Serial.printf("%s CH%u APDS ID=0x%02X ready\n", sensor.name,
                unsigned(sensor.channel), unsigned(id));
  return true;
}

void printSensor(Sensor &sensor) {
  uint8_t raw = 0;
  const bool ok = selectChannel(sensor.channel) && readRegister(REG_PDATA, raw);
  Serial.printf("%s CH%u ", sensor.name, unsigned(sensor.channel));
  if (!ok) {
    Serial.print("READ_ERROR");
    return;
  }
  sensor.filtered = sensor.hasSample
                        ? FILTER_ALPHA * sensor.filtered + (1.0f - FILTER_ALPHA) * raw
                        : float(raw);
  sensor.hasSample = true;
  Serial.printf("raw=%3u filtered=%6.1f%s", unsigned(raw), sensor.filtered,
                raw == 255 ? " SATURATED" : "");
}

void setup() {
  // Keep the TB6612 disabled throughout this sensor-only test.
  pinMode(MOTOR_STBY_PIN, OUTPUT);
  digitalWrite(MOTOR_STBY_PIN, LOW);
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(20);
  Serial.println("All APDS-9960 test: LEFT CH0, RIGHT CH1, FRONT CH6");
  Serial.printf("ESP32 SDA=%u SCL=%u, TCA=0x%02X, APDS=0x%02X, gain=%ux\n",
                unsigned(SDA_PIN), unsigned(SCL_PIN), unsigned(TCA_ADDRESS),
                unsigned(APDS_ADDRESS), unsigned(APDS_GAIN));
  for (Sensor &sensor : sensors) {
    if (!initSensor(sensor)) {
      Serial.printf("%s CH%u INIT_ERROR: check TCA channel, power and wiring\n",
                    sensor.name, unsigned(sensor.channel));
    }
  }
}

void loop() {
  for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); ++i) {
    if (i != 0) Serial.print(" | ");
    printSensor(sensors[i]);
  }
  Serial.println();
  delay(200);
}
