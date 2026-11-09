#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS7341.h>

// HARDWARE 1 - Wiring map. The three motor pins are IN1, IN2, and PWM.
constexpr int SDA_PIN = 21, SCL_PIN = 22;
constexpr int LEFT_FORWARD = 25, LEFT_REVERSE = 26, LEFT_SPEED = 27;
constexpr int RIGHT_FORWARD = 18, RIGHT_REVERSE = 19, RIGHT_SPEED = 23;
constexpr int MOTOR_STANDBY = 33;

// I2C addresses are separate from the TCA's numbered channels.
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

// HARDWARE 2 - I2C communication. Select a TCA channel before reading its device.
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

// HARDWARE 3 - Motor driver. STBY LOW turns off both motor outputs.
void stopMotors() {
  digitalWrite(MOTOR_STANDBY, LOW);
  analogWrite(LEFT_SPEED, 0);
  analogWrite(RIGHT_SPEED, 0);
  for (int pin : {LEFT_FORWARD, LEFT_REVERSE, RIGHT_FORWARD, RIGHT_REVERSE}) {
    digitalWrite(pin, LOW);
  }
  leftPWM = rightPWM = 0;
}

void setupMotorPins() {
  pinMode(MOTOR_STANDBY, OUTPUT);
  digitalWrite(MOTOR_STANDBY, LOW);
  for (int pin : {LEFT_FORWARD, LEFT_REVERSE, LEFT_SPEED,
                  RIGHT_FORWARD, RIGHT_REVERSE, RIGHT_SPEED}) {
    pinMode(pin, OUTPUT);
  }
  stopMotors();
}

// Positive speed drives forward. Negative speed drives backward.
void moveMotor(int left, int right) {
  leftPWM = int(constrain(left, -255, 255) * 0.985f);
  rightPWM = constrain(right, -255, 255);
  analogWrite(LEFT_SPEED, 0);
  analogWrite(RIGHT_SPEED, 0);
  digitalWrite(LEFT_FORWARD, leftPWM > 0 ? HIGH : LOW);
  digitalWrite(LEFT_REVERSE, leftPWM < 0 ? HIGH : LOW);
  digitalWrite(RIGHT_FORWARD, rightPWM > 0 ? HIGH : LOW);
  digitalWrite(RIGHT_REVERSE, rightPWM < 0 ? HIGH : LOW);
  analogWrite(LEFT_SPEED, abs(leftPWM));
  analogWrite(RIGHT_SPEED, abs(rightPWM));
  digitalWrite(MOTOR_STANDBY, HIGH);
}

// HARDWARE 4 - APDS wall sensors. Read a proximity count for each side.
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
  // PDATA is the sensor's one-byte proximity reading (0 to 255).
  if (!selectChannel(sensor.channel) || !readReg(APDS_ADDR, 0x9C, &sensor.raw, 1)) {
    sensor.filterReady = false;
    return false;
  }
  sensor.filtered = sensor.filterReady ? APDS_ALPHA * sensor.filtered +
      (1.0f - APDS_ALPHA) * sensor.raw : sensor.raw;
  sensor.filterReady = true;
  return true;
}

// HARDWARE 5 - Wheel encoders. Track how far each wheel has turned.
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
    // Follow the wheel across the 4095-to-0 boundary.
    if (delta > 2048) delta -= 4096;
    if (delta < -2048) delta += 4096;
    encoder.counts += delta;
  }
  encoder.previous = angle;
  return true;
}

bool encoderUsable(const Encoder &encoder) {
  // Require a successful read and a detected magnet within range.
  return encoder.readOK && (encoder.status & 0x38) == 0x20;
}

// HARDWARE 6 - Gyro. Measure rotation and add it up to get the heading.
// The original mpu.begin(3, 0) selects the +/-2000 degrees/second range.
constexpr uint8_t GYRO_RANGE_SETTING = 0x18;
constexpr uint8_t ACCEL_RANGE_SETTING = 0x00;  // Original +/-2 g range.
constexpr float GYRO_COUNTS_PER_DPS = 16.4f;
constexpr int GYRO_CALIBRATION_SAMPLES = 500;

bool readGyroZ(float &rate) {
  uint8_t data[2];
  if (!readReg(imuAddress, 0x47, data, sizeof(data))) return false;
  const int16_t rawRate = int16_t((uint16_t(data[0]) << 8) | data[1]);
  rate = rawRate / GYRO_COUNTS_PER_DPS;
  return true;
}

bool initIMU() {
  // 1. Find the IMU on the main bus and read its model ID.
  if (!selectMux(0)) {
    Serial.println("IMU ERROR: could not close the TCA channels before checking the main bus.");
    return false;
  }
  for (uint8_t address : {uint8_t(0x68), uint8_t(0x69)}) {
    uint8_t id = 0;
    if (!readReg(address, 0x75, &id, 1)) continue;
    if (id != 0x68 && id != 0x70) {
      Serial.printf("IMU at 0x%02X returned unsupported ID 0x%02X.\n", unsigned(address), unsigned(id));
      continue;
    }
    imuAddress = address;
    Serial.printf("IMU %s at 0x%02X, ID=0x%02X.\n",
                  id == 0x68 ? "MPU6050" : "MPU6500", unsigned(address), unsigned(id));

    // 2. Wake the chip, apply the original settings, and verify both ranges.
    if (!writeReg(address, 0x6B, 0x01)) {
      Serial.println("IMU ERROR: wake command failed.");
      return false;
    }
    delay(100);
    if (!writeReg(address, 0x6C, 0x00) || !writeReg(address, 0x19, 0x00) ||
        !writeReg(address, 0x1A, 0x00) || !writeReg(address, 0x1B, GYRO_RANGE_SETTING) ||
        !writeReg(address, 0x1C, ACCEL_RANGE_SETTING)) {
      Serial.println("IMU ERROR: configuration write failed.");
      return false;
    }
    uint8_t gyroConfig = 0;
    if (!readReg(address, 0x1B, &gyroConfig, 1) || (gyroConfig & 0x18) != GYRO_RANGE_SETTING) {
      Serial.println("IMU ERROR: gyro range setting did not read back.");
      return false;
    }
    uint8_t accelConfig = 0;
    if (!readReg(address, 0x1C, &accelConfig, 1) || (accelConfig & 0x18) != ACCEL_RANGE_SETTING) {
      Serial.println("IMU ERROR: accelerometer range setting did not read back.");
      return false;
    }
    Serial.println("Gyro range +/-2000 deg/s. Accelerometer range +/-2 g. Keep still.");

    // 3. Average stationary Z readings to measure the gyro's zero offset.
    float minimum = 2000, maximum = -2000, total = 0;
    for (int i = 0; i < GYRO_CALIBRATION_SAMPLES; ++i) {
      float rate;
      if (!readGyroZ(rate)) {
        Serial.println("IMU ERROR: gyro read failed during calibration.");
        return false;
      }
      minimum = min(minimum, rate);
      maximum = max(maximum, rate);
      total += rate;
      delay(1);
    }
    if (maximum - minimum > 5.0f) {
      Serial.println("IMU ERROR: gyro changed during calibration. Hold still and reset.");
      return false;
    }
    gyroBias = total / GYRO_CALIBRATION_SAMPLES;
    lastGyroSample = micros();
    return true;
  }
  Serial.println("IMU ERROR: no MPU6050 or MPU6500 found at 0x68 or 0x69. Check the main I2C wiring.");
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

// HARDWARE 7 - Floor color sensor and its light.
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
