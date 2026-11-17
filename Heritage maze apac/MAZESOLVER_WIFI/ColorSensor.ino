// Bottom-facing AS7341 and its illumination LED.
#include <Adafruit_AS7341.h>

Adafruit_AS7341 colorSensor;
bool colorReady = false;

// Same ADC mappings as the Adafruit AS7341 library, written as two RAM blocks.
// https://github.com/adafruit/Adafruit_AS7341/blob/master/Adafruit_AS7341.cpp
constexpr uint8_t COLOR_MUX_LOW[] = {
    0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,
    0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06,
};
constexpr uint8_t COLOR_MUX_HIGH[] = {
    0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,
    0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06,
};
constexpr uint8_t COLOR_POWER_ON = 0x01;
constexpr uint8_t COLOR_SPECTRAL_ENABLE = 0x02;
constexpr uint8_t COLOR_MUX_ENABLE = 0x10;
constexpr uint8_t COLOR_DATA_VALID = 0x40;
constexpr uint32_t COLOR_POLL_MS = 5;
constexpr uint32_t COLOR_INTEGRATION_MS = 55; // ATIME 29 and ASTEP 599 take about 50 ms.
constexpr uint32_t COLOR_BANK_TIMEOUT_MS = 150;

void setupColorSensor() {
  colorReady = TCA9548A(COLOR_CH) &&
               colorSensor.begin(AS7341_I2CADDR_DEFAULT, &Wire) &&
               colorSensor.setATIME(29) && colorSensor.setASTEP(599) &&
               colorSensor.setGain(AS7341_GAIN_16X);
  if (!colorReady) {
    SerialDebug.printf("AS7341 CH%u unavailable. Check SDA, SCL, power and ground.\n", unsigned(COLOR_CH));
    return;
  }
  if (!colorSensor.setLEDCurrent(4) || !colorSensor.enableLED(true)) {
    SerialDebug.println("AS7341: LED enable failed.");
  }
  SerialDebug.printf("AS7341 CH%u ready. Send c before the left hand gesture to calibrate.\n",
                unsigned(COLOR_CH));
}

bool writeColorBytes(uint8_t reg, const uint8_t *data, size_t size) {
  Wire.beginTransmission(uint8_t(AS7341_I2CADDR_DEFAULT));
  Wire.write(reg);
  Wire.write(data, size);
  return Wire.endTransmission() == 0;
}

bool writeColorRegister(uint8_t reg, uint8_t value) {
  return writeColorBytes(reg, &value, 1);
}

bool readColorBytes(uint8_t reg, uint8_t *data, size_t size) {
  const uint8_t address = AS7341_I2CADDR_DEFAULT;
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, size) != size) return false;
  for (size_t i = 0; i < size; ++i) data[i] = uint8_t(Wire.read());
  return true;
}

bool startColorBank(bool highBank) {
  return writeColorRegister(AS7341_ENABLE, COLOR_POWER_ON) &&
         writeColorRegister(AS7341_CFG6, uint8_t(AS7341_SMUX_CMD_WRITE) << 3) &&
         writeColorBytes(0x00, highBank ? COLOR_MUX_HIGH : COLOR_MUX_LOW, sizeof(COLOR_MUX_LOW)) &&
         writeColorRegister(AS7341_ENABLE, COLOR_POWER_ON | COLOR_MUX_ENABLE);
}

// Take one step at a time so the gyro and wall sensors run between integrations.
ColorSampleStatus pollColorSample(uint16_t *readings) {
  enum class Phase { Idle, Configure, Integrate };
  static Phase phase = Phase::Idle;
  static bool highBank = false;
  static uint32_t lastPoll = 0, phaseStarted = 0;
  static uint16_t lowBands[4];
  const uint32_t now = millis();
  if (now - lastPoll < COLOR_POLL_MS) return ColorSampleStatus::Waiting;
  const bool stale = phase != Phase::Idle &&
      (now - lastPoll > 100 || now - phaseStarted > COLOR_BANK_TIMEOUT_MS);
  lastPoll = now;
  if (stale || !colorReady) {
    phase = Phase::Idle;
    return ColorSampleStatus::Failed;
  }
  if (phase == Phase::Integrate && now - phaseStarted < COLOR_INTEGRATION_MS) {
    return ColorSampleStatus::Waiting;
  }
  if (!TCA9548A(COLOR_CH)) {
    phase = Phase::Idle;
    return ColorSampleStatus::Failed;
  }

  uint8_t status;
  if (phase == Phase::Idle) {
    if (!startColorBank(false)) return ColorSampleStatus::Failed;
    highBank = false;
    phase = Phase::Configure;
    phaseStarted = millis();
    return ColorSampleStatus::Waiting;
  }
  if (phase == Phase::Configure) {
    if (readColorBytes(AS7341_ENABLE, &status, 1)) {
      if (status & COLOR_MUX_ENABLE) return ColorSampleStatus::Waiting;
      if (writeColorRegister(AS7341_ENABLE, COLOR_POWER_ON | COLOR_SPECTRAL_ENABLE)) {
        phase = Phase::Integrate;
        phaseStarted = millis();
        return ColorSampleStatus::Waiting;
      }
    }
    phase = Phase::Idle;
    return ColorSampleStatus::Failed;
  }

  if (!readColorBytes(AS7341_STATUS2, &status, 1)) {
    phase = Phase::Idle;
    return ColorSampleStatus::Failed;
  }
  if (!(status & COLOR_DATA_VALID)) return ColorSampleStatus::Waiting;
  uint8_t data[12];
  phase = Phase::Idle;
  if (!readColorBytes(AS7341_CH0_DATA_L, data, sizeof(data))) return ColorSampleStatus::Failed;
  if (!highBank) {
    for (size_t i = 0; i < 4; ++i) lowBands[i] = uint16_t(data[2 * i + 1]) << 8 | data[2 * i];
    if (!startColorBank(true)) return ColorSampleStatus::Failed;
    highBank = true;
    phase = Phase::Configure;
    phaseStarted = millis();
    return ColorSampleStatus::Waiting;
  }
  for (size_t i = 0; i < 4; ++i) readings[i] = lowBands[i];
  for (size_t i = 0; i < 6; ++i) readings[i + 4] = uint16_t(data[2 * i + 1]) << 8 | data[2 * i];
  return ColorSampleStatus::Ready;
}

// Calibration can wait for a complete sample while the motors are stopped.
bool readColorSample(uint16_t *readings) {
  if (!colorReady) return false;
  ColorSampleStatus result;
  do {
    result = pollColorSample(readings);
    if (result == ColorSampleStatus::Waiting) delay(COLOR_POLL_MS);
  } while (result == ColorSampleStatus::Waiting);
  return result == ColorSampleStatus::Ready;
}
