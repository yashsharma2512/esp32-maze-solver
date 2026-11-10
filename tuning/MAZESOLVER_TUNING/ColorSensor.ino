// Bottom-facing AS7341 and its illumination LED.
#include <Adafruit_AS7341.h>

Adafruit_AS7341 colorSensor;
bool colorReady = false;

constexpr as7341_color_channel_t COLOR_BANDS[COLOR_BAND_COUNT] = {
    AS7341_CHANNEL_415nm_F1, AS7341_CHANNEL_445nm_F2, AS7341_CHANNEL_480nm_F3,
    AS7341_CHANNEL_515nm_F4, AS7341_CHANNEL_555nm_F5, AS7341_CHANNEL_590nm_F6,
    AS7341_CHANNEL_630nm_F7, AS7341_CHANNEL_680nm_F8,
    AS7341_CHANNEL_CLEAR, AS7341_CHANNEL_NIR,
};

void setupColorSensor() {
  colorReady = TCA9548A(COLOR_CH) &&
               colorSensor.begin(AS7341_I2CADDR_DEFAULT, &Wire) &&
               colorSensor.setATIME(29) && colorSensor.setASTEP(599) &&
               colorSensor.setGain(AS7341_GAIN_16X);
  if (!colorReady) {
    Serial.printf("AS7341 CH%u unavailable. Check SDA, SCL, power and ground.\n", unsigned(COLOR_CH));
    return;
  }
  if (!colorSensor.setLEDCurrent(4) || !colorSensor.enableLED(true)) {
    Serial.println("AS7341: LED enable failed.");
  }
  Serial.printf("AS7341 CH%u ready. Send c before the left hand gesture to calibrate.\n",
                unsigned(COLOR_CH));
}

// Color measurements run during the startup wait with the motors in standby.
bool readColorSample(uint16_t *readings) {
  if (!colorReady || !TCA9548A(COLOR_CH) || !colorSensor.startReading()) return false;
  const uint32_t started = millis();
  while (!colorSensor.checkReadingProgress()) {
    if (millis() - started >= 1000) return false;
    delay(5);
  }
  for (size_t i = 0; i < COLOR_BAND_COUNT; ++i) {
    readings[i] = colorSensor.getChannel(COLOR_BANDS[i]);
  }
  return true;
}
