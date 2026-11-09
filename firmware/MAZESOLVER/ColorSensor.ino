// AS7341 - Floor color and illumination on TCA CH2.
Adafruit_AS7341 colorSensor;
bool colorReady = false;
uint32_t lastColorSample = 0;

constexpr as7341_color_channel_t COLOR_BANDS[COLOR_BAND_COUNT] = {
    AS7341_CHANNEL_415nm_F1, AS7341_CHANNEL_445nm_F2, AS7341_CHANNEL_480nm_F3,
    AS7341_CHANNEL_515nm_F4, AS7341_CHANNEL_555nm_F5, AS7341_CHANNEL_590nm_F6,
    AS7341_CHANNEL_630nm_F7, AS7341_CHANNEL_680nm_F8,
    AS7341_CHANNEL_CLEAR, AS7341_CHANNEL_NIR,
};

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

void setupColorSensor() {
  colorReady = initColor();
  Serial.println(colorReady ? "AS7341 CH2 ready. Color samples print while stopped."
                            : "AS7341 CH2 init failed. Check SD2/SC2.");
}

bool readColorSample(uint16_t *readings) {
  if (!colorReady || !sampleColor()) return false;
  for (size_t i = 0; i < COLOR_BAND_COUNT; ++i) {
    readings[i] = colorSensor.getChannel(COLOR_BANDS[i]);
  }
  return true;
}

void printColorSample() {
  Serial.print("AS7341 F1..F8,Clear,NIR:");
  for (auto band : COLOR_BANDS) Serial.printf(" %u", unsigned(colorSensor.getChannel(band)));
  Serial.println();
}

void reportColorWhileStopped() {
  if (colorReady && millis() - lastColorSample >= 600) {
    if (sampleColor()) printColorSample();
    else Serial.println("AS7341 CH2 read timed out or channel selection failed.");
    lastColorSample = millis();
  }
}
