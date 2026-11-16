// Bottom-facing APDS9960. The wall APDS sensors retain their original setup.
constexpr uint8_t COLOR_ATIME_REG = 0x81;
constexpr uint8_t COLOR_ID_REG = 0x92;
constexpr uint8_t COLOR_STATUS_REG = 0x93;
constexpr uint8_t COLOR_DATA_REG = 0x94;
constexpr uint8_t COLOR_CLEAR_INTERRUPT = 0xE6;
constexpr uint8_t COLOR_POWER_ON = 0x01;
constexpr uint8_t COLOR_ADC_ENABLE = 0x02;
constexpr uint8_t COLOR_DATA_VALID = 0x01;
constexpr uint8_t COLOR_SATURATED = 0x80;
constexpr uint32_t COLOR_POLL_MS = 30;  // One APDS RGBC integration takes 27.8 ms.

bool colorReady = false;

bool writeColorRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readColorBytes(uint8_t reg, uint8_t *data, size_t size) {
  const uint8_t address = APDS9960_ADDRESS;
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, size) != size) return false;
  for (size_t i = 0; i < size; ++i) data[i] = uint8_t(Wire.read());
  return true;
}

void setupColorSensor() {
  uint8_t id = 0;
  colorReady = TCA9548A(COLOR_CH) &&
               readColorBytes(COLOR_ID_REG, &id, 1) && id == 0xAB &&
               writeColorRegister(APDS9960_REG_ENABLE, COLOR_POWER_ON) &&
               writeColorRegister(COLOR_ATIME_REG, 0xF6) &&
               writeColorRegister(APDS9960_REG_CONTROL, 0x01);  // 4x color gain.
  if (colorReady) {
    delay(5);
    colorReady = writeColorRegister(APDS9960_REG_ENABLE, COLOR_POWER_ON | COLOR_ADC_ENABLE);
  }
  if (colorReady) {
    SerialDebug.printf("APDS9960 CH%u ready. Send c before the left hand gesture to calibrate.\n",
                       unsigned(COLOR_CH));
  } else {
    SerialDebug.printf("APDS9960 CH%u unavailable. Check SDA, SCL, power and ground.\n",
                       unsigned(COLOR_CH));
  }
}

ColorSampleStatus pollColorSample(uint16_t *readings) {
  static uint32_t lastPoll = 0;
  const uint32_t now = millis();
  if (now - lastPoll < COLOR_POLL_MS) return ColorSampleStatus::Waiting;
  lastPoll = now;

  uint8_t status;
  if (!colorReady || !TCA9548A(COLOR_CH) || !readColorBytes(COLOR_STATUS_REG, &status, 1)) {
    return ColorSampleStatus::Failed;
  }
  if (status & COLOR_SATURATED) {
    Wire.beginTransmission(APDS9960_ADDRESS);
    Wire.write(COLOR_CLEAR_INTERRUPT);
    Wire.endTransmission();
    return ColorSampleStatus::Failed;
  }
  if (!(status & COLOR_DATA_VALID)) return ColorSampleStatus::Waiting;
  uint8_t data[2 * COLOR_BAND_COUNT];
  if (!readColorBytes(COLOR_DATA_REG, data, sizeof(data))) return ColorSampleStatus::Failed;
  for (size_t i = 0; i < COLOR_BAND_COUNT; ++i) {
    readings[i] = (uint16_t(data[2 * i + 1]) << 8) | data[2 * i];
    if (readings[i] >= 10240) return ColorSampleStatus::Failed;  // ADC limit at this ATIME.
  }
  return ColorSampleStatus::Ready;
}

// Calibration can wait for a complete sample while the motors are stopped.
bool readColorSample(uint16_t *readings) {
  const uint32_t started = millis();
  if (!colorReady) return false;
  do {
    const ColorSampleStatus result = pollColorSample(readings);
    if (result != ColorSampleStatus::Waiting) return result == ColorSampleStatus::Ready;
    delay(COLOR_POLL_MS);
  } while (millis() - started < 150);
  return false;
}
