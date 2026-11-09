// ESP32 Dev Module: test all three APDS-9960 proximity sensors.
#include <Arduino.h>
#include <Wire.h>

constexpr int SDA_PIN = 21, SCL_PIN = 22, MOTOR_STBY = 33;
constexpr uint8_t TCA_ADDR = 0x70, APDS_ADDR = 0x39;
constexpr uint32_t SAMPLE_MS = 20, PRINT_MS = 250;
constexpr float FILTER_ALPHA = 0.65f;
// Match firmware/MAZESOLVER/MAZESOLVER.ino.
constexpr uint8_t SIDE_OPEN = 9, SIDE_PRESENT = 25, FRONT_BLOCKED = 40;
constexpr uint8_t HAND_HIGH = 150, HAND_RELEASE = 100;

struct ProximitySensor {
  const char *name;
  uint8_t channel;
  bool ready = false;
  bool readOK = false;
  bool filterReady = false;
  uint8_t raw = 0, status = 0;
  float filtered = 0;
};

ProximitySensor left{"LEFT", 0}, right{"RIGHT", 1}, front{"FRONT", 7};
ProximitySensor *const sensors[] = {&left, &right, &front};
bool busReady = false;

bool selectMux(uint8_t mask) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(mask);
  if (Wire.endTransmission() != 0) return false;
  return Wire.requestFrom(TCA_ADDR, uint8_t(1)) == 1 && Wire.read() == mask;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(APDS_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(APDS_ADDR, uint8_t(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool initAPDS(const ProximitySensor &sensor) {
  uint8_t id = 0;
  if (!selectMux(uint8_t(1U << sensor.channel)) || !readRegister(0x92, id)) {
    Serial.printf("%s CH%u INIT ERROR: no APDS response.\n", sensor.name, unsigned(sensor.channel));
    return false;
  }
  if (id != 0xAB) {
    Serial.printf("%s CH%u INIT ERROR: ID=0x%02X, expected 0xAB.\n",
                  sensor.name, unsigned(sensor.channel), unsigned(id));
    return false;
  }

  // Apply the same gain, LED boost, and eight 8-us pulses as the maze firmware.
  if (!writeRegister(0x80, 0x00) || !writeRegister(0x90, 0x31) ||
      !writeRegister(0x8F, 0x49) || !writeRegister(0x8E, 0x87) ||
      !writeRegister(0x80, 0x01)) {
    Serial.printf("%s CH%u INIT ERROR: configuration write failed.\n",
                  sensor.name, unsigned(sensor.channel));
    return false;
  }
  delay(10);
  uint8_t enable = 0, pulse = 0, control = 0, config2 = 0;
  if (!writeRegister(0x80, 0x05) || !readRegister(0x80, enable) ||
      !readRegister(0x8E, pulse) || !readRegister(0x8F, control) ||
      !readRegister(0x90, config2)) {
    Serial.printf("%s CH%u INIT ERROR: proximity enable or readback failed.\n",
                  sensor.name, unsigned(sensor.channel));
    return false;
  }
  const bool ready = enable == 0x05 && pulse == 0x87 && control == 0x49 && config2 == 0x31;
  Serial.printf("%s CH%u %s ID=0x%02X ENABLE=0x%02X PPULSE=0x%02X CONTROL=0x%02X CONFIG2=0x%02X\n",
                sensor.name, unsigned(sensor.channel), ready ? "READY" : "CONFIG MISMATCH",
                unsigned(id), unsigned(enable), unsigned(pulse), unsigned(control), unsigned(config2));
  return ready;
}

void sampleAPDS(ProximitySensor &sensor) {
  if (!sensor.ready) return;
  sensor.readOK = selectMux(uint8_t(1U << sensor.channel)) &&
                  readRegister(0x93, sensor.status) && readRegister(0x9C, sensor.raw);
  if (!sensor.readOK) {
    sensor.filterReady = false;
    return;
  }
  if (sensor.status & 0x40) {
    // Exclude saturation from steering data, then clear the latched flag.
    sensor.filterReady = false;
    Wire.beginTransmission(APDS_ADDR);
    Wire.write(uint8_t(0xE5));
    sensor.readOK = Wire.endTransmission() == 0;
    return;
  }
  // PVALID is informational here. Match the main firmware's PDATA handling.
  sensor.filtered = sensor.filterReady ? FILTER_ALPHA * sensor.filtered +
      (1.0f - FILTER_ALPHA) * sensor.raw : sensor.raw;
  sensor.filterReady = true;
}

void printSample(const ProximitySensor &sensor) {
  Serial.printf("%s CH%u ", sensor.name, unsigned(sensor.channel));
  if (!sensor.ready || !sensor.readOK) {
    Serial.println(sensor.ready ? "READ ERROR" : "INIT ERROR");
    return;
  }
  Serial.printf("raw=%3u valid=%u sat=%u", unsigned(sensor.raw),
                unsigned(bool(sensor.status & 0x02)), unsigned(bool(sensor.status & 0x40)));
  if (&sensor != &front) {
    const char *hand = "BETWEEN";
    if (sensor.raw > HAND_HIGH) hand = "HIGH";
    if (sensor.raw < HAND_RELEASE) hand = "RELEASED";
    Serial.printf(" hand=%s", hand);
  }
  if (!sensor.filterReady) {
    Serial.println(" filtered=-- (saturated)");
    return;
  }
  Serial.printf(" filtered=%5.1f", sensor.filtered);
  if (&sensor == &front) {
    const bool blocked = sensor.raw > FRONT_BLOCKED;
    Serial.printf(" front=%s\n", blocked ? "BLOCKED" : "CLEAR");
    return;
  }
  const char *wall = "BETWEEN";
  if (sensor.filtered <= SIDE_OPEN) wall = "OPEN";
  if (int(sensor.filtered) > SIDE_PRESENT) wall = "WALL";
  Serial.printf(" side=%s\n", wall);
}

void setup() {
  pinMode(MOTOR_STBY, OUTPUT);
  digitalWrite(MOTOR_STBY, LOW);
  Serial.begin(115200);
  delay(750);
  Serial.println("\nAPDS-9960 ALL-SENSOR TEST | LEFT CH0 | RIGHT CH1 | FRONT CH7");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("ERROR: could not start I2C.");
    return;
  }
  Wire.setTimeOut(20);
  if (!selectMux(0)) {
    Serial.println("ERROR: TCA9548A at 0x70 did not respond. Check GPIO21/22 and power.");
    return;
  }
  Wire.beginTransmission(APDS_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("ERROR: a device at 0x39 is on the main bus. Check that AS7341 is on CH2.");
    return;
  }
  busReady = true;
  for (auto *sensor : sensors) sensor->ready = initAPDS(*sensor);
  Serial.println("Move a hand or wall sample toward each sensor separately.");
  Serial.println("Side: OPEN <=9, WALL >=26. Front: raw BLOCKED >40. Hand: HIGH >150, RELEASED <100.");
}

void loop() {
  static uint32_t lastSample = 0, lastPrint = 0;
  const uint32_t now = millis();
  if (busReady && now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    for (auto *sensor : sensors) sampleAPDS(*sensor);
  }
  if (busReady && now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    for (const auto *sensor : sensors) printSample(*sensor);
    Serial.println();
  }
  delay(1);
}
