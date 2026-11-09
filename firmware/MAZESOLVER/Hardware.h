#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS7341.h>

constexpr int SDA_PIN = 21, SCL_PIN = 22;
constexpr int LMF = 25, LMR = 26, PWML = 27;
constexpr int RMF = 18, RMR = 19, PWMR = 23, STBY = 33;
constexpr uint8_t TCA_ADDR = 0x70, APDS_ADDR = 0x39, AS5600_ADDR = 0x36;
constexpr uint8_t COLOR_CH = 2;
constexpr float APDS_ALPHA = 0.65f;

struct Proximity {
  const char *name;
  uint8_t channel;
  uint8_t raw = 0;
  float filtered = 0;
  bool filterReady = false;
};
Proximity leftAPDS{"left", 0}, rightAPDS{"right", 1}, frontAPDS{"front", 7};

struct Encoder {
  uint8_t channel;
  int16_t previous = -1;
  int32_t counts = 0;
  uint8_t status = 0;
  bool readOK = false;
};
Encoder leftEncoder{5}, rightEncoder{4};

Adafruit_AS7341 colorSensor;
bool colorReady = false;
uint32_t lastColorSample = 0;
uint8_t imuAddress = 0;
float gyroBias = 0, yaw = 0;
uint32_t lastGyroSample = 0;
int leftPWM = 0, rightPWM = 0;

bool selectMux(uint8_t mask) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(mask);
  if (Wire.endTransmission() != 0) return false;
  return Wire.requestFrom(TCA_ADDR, uint8_t(1)) == 1 && Wire.read() == mask;
}

bool selectChannel(uint8_t channel) {
  return selectMux(uint8_t(1U << channel));
}

bool readReg(uint8_t address, uint8_t reg, uint8_t *out, uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, length) != length) return false;
  for (uint8_t i = 0; i < length; ++i) out[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void stopMotors() {
  digitalWrite(STBY, LOW);
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  for (int pin : {LMF, LMR, RMF, RMR}) digitalWrite(pin, LOW);
  leftPWM = rightPWM = 0;
}

void setMotorIOs() {
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);
  for (int pin : {LMF, LMR, PWML, RMF, RMR, PWMR}) pinMode(pin, OUTPUT);
  stopMotors();
}

// Same signed PWM convention and left trim as the original maze sketch.
void moveMotor(int left, int right) {
  leftPWM = int(constrain(left, -255, 255) * 0.985f);
  rightPWM = constrain(right, -255, 255);
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(LMF, leftPWM > 0 ? HIGH : LOW);
  digitalWrite(LMR, leftPWM < 0 ? HIGH : LOW);
  digitalWrite(RMF, rightPWM > 0 ? HIGH : LOW);
  digitalWrite(RMR, rightPWM < 0 ? HIGH : LOW);
  analogWrite(PWML, abs(leftPWM));
  analogWrite(PWMR, abs(rightPWM));
  digitalWrite(STBY, HIGH);
}

bool initAPDS(const Proximity &sensor) {
  uint8_t id = 0, enable = 0, pulse = 0;
  if (!selectChannel(sensor.channel) || !readReg(APDS_ADDR, 0x92, &id, 1) ||
      id != 0xAB) return false;
  // Match the working APDS test, including the eight IR pulses.
  if (!writeReg(APDS_ADDR, 0x80, 0x00) || !writeReg(APDS_ADDR, 0x90, 0x31) ||
      !writeReg(APDS_ADDR, 0x8F, 0x49) || !writeReg(APDS_ADDR, 0x8E, 0x87) ||
      !writeReg(APDS_ADDR, 0x80, 0x01)) return false;
  delay(10);
  return writeReg(APDS_ADDR, 0x80, 0x05) &&
         readReg(APDS_ADDR, 0x80, &enable, 1) && enable == 0x05 &&
         readReg(APDS_ADDR, 0x8E, &pulse, 1) && pulse == 0x87;
}

bool sampleAPDS(Proximity &sensor) {
  if (!selectChannel(sensor.channel) || !readReg(APDS_ADDR, 0x9C, &sensor.raw, 1)) {
    sensor.filterReady = false;
    return false;
  }
  sensor.filtered = sensor.filterReady ? APDS_ALPHA * sensor.filtered +
      (1.0f - APDS_ALPHA) * sensor.raw : sensor.raw;
  sensor.filterReady = true;
  return true;
}

bool sampleEncoder(Encoder &encoder) {
  uint8_t data[3]; // STATUS, RAW_ANGLE_H, RAW_ANGLE_L
  encoder.readOK = selectChannel(encoder.channel) &&
                   readReg(AS5600_ADDR, 0x0B, data, sizeof(data));
  if (!encoder.readOK) {
    encoder.previous = -1;
    return false;
  }
  encoder.status = data[0];
  int16_t angle = ((data[1] & 0x0F) << 8) | data[2];
  if (encoder.previous >= 0) {
    int delta = angle - encoder.previous;
    if (delta > 2048) delta -= 4096;
    if (delta < -2048) delta += 4096;
    encoder.counts += delta;
  }
  encoder.previous = angle;
  return true;
}

bool encoderUsable(const Encoder &encoder) {
  return encoder.readOK && (encoder.status & 0x38) == 0x20;
}

bool readGyroZ(float &rate) {
  uint8_t data[2];
  if (!readReg(imuAddress, 0x47, data, sizeof(data))) return false;
  rate = int16_t((uint16_t(data[0]) << 8) | data[1]) / 131.0f;
  return true;
}

bool initIMU() {
  for (uint8_t address : {uint8_t(0x68), uint8_t(0x69)}) {
    uint8_t id = 0;
    if (!readReg(address, 0x75, &id, 1) || (id != 0x70 && id != 0x68)) continue;
    imuAddress = address;
    if (!writeReg(address, 0x6B, 0x01)) return false;
    delay(100);
    if (!writeReg(address, 0x6C, 0x00) || !writeReg(address, 0x1B, 0x00) ||
        !writeReg(address, 0x1A, 0x03)) return false;
    Serial.printf("IMU 0x%02X ID=0x%02X. Keep the robot still for gyro calibration.\n",
                  unsigned(address), unsigned(id));
    float minimum = 250, maximum = -250, total = 0;
    for (int i = 0; i < 120; ++i) {
      float rate;
      if (!readGyroZ(rate)) return false;
      minimum = min(minimum, rate);
      maximum = max(maximum, rate);
      total += rate;
      delay(5);
    }
    if (maximum - minimum > 5.0f) return false;
    gyroBias = total / 120.0f;
    lastGyroSample = micros();
    return true;
  }
  return false;
}

bool sampleIMU() {
  float rate;
  if (!readGyroZ(rate)) return false;
  const uint32_t now = micros();
  const uint32_t elapsed = now - lastGyroSample;
  if (elapsed <= 120000) yaw += (rate - gyroBias) * (elapsed / 1000000.0f);
  lastGyroSample = now;
  return true;
}

bool initColor() {
  if (!selectChannel(COLOR_CH) || !colorSensor.begin(AS7341_I2CADDR_DEFAULT, &Wire)) {
    return false;
  }
  if (!colorSensor.setATIME(29) || !colorSensor.setASTEP(599) ||
      !colorSensor.setGain(AS7341_GAIN_16X)) return false;
  if (!colorSensor.setLEDCurrent(4) || !colorSensor.enableLED(true)) {
    Serial.println("AS7341: LED enable failed.");
  }
  return true;
}

// Only called in standby. Adafruit's internal SMUX setup can wait on the chip.
bool sampleColor() {
  if (!selectChannel(COLOR_CH)) return false;
  const uint32_t started = millis();
  colorSensor.startReading();
  while (millis() - started < 1000) {
    if (colorSensor.checkReadingProgress()) return true;
    delay(5);
  }
  return false;
}
