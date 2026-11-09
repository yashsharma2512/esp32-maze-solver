// ESP32 Dev Module. Install Adafruit AS7341 and Adafruit BusIO.
// AS7341 on the main I2C bus, GPIO21 SDA and GPIO22 SCL.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS7341.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr uint8_t TCA_ADDRESS = 0x70;
constexpr uint32_t SAMPLE_TIMEOUT_MS = 1000;

Adafruit_AS7341 colorSensor;
bool sensorReady = false;

// AS7341 and APDS-9960 both use 0x39. Keep all APDS branches isolated.
bool isolateMuxChannels() {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(uint8_t(0));
  const uint8_t result = Wire.endTransmission();
  if (result == 2) {
    Serial.println("No TCA at 0x70. Continuing with the main bus only.");
    return true;
  }
  if (result != 0 || Wire.requestFrom(TCA_ADDRESS, uint8_t(1)) != 1 ||
      Wire.read() != 0) {
    Serial.println("ERROR: Could not disable the TCA channels.");
    return false;
  }
  Serial.println("TCA channels disabled.");
  return true;
}

bool readSpectrum() {
  const uint32_t started = millis();
  colorSensor.startReading();
  while (millis() - started < SAMPLE_TIMEOUT_MS) {
    if (colorSensor.checkReadingProgress()) return true;
    delay(5);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP32 AS7341 main-bus test");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("ERROR: Could not start I2C.");
    return;
  }
  Wire.setTimeOut(50);
  if (!isolateMuxChannels()) return;

  if (!colorSensor.begin(AS7341_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("ERROR: AS7341 at 0x39 did not respond with the expected ID.");
    Serial.println("Check power, GND, GPIO21/22 and any other device at 0x39.");
    return;
  }
  if (!colorSensor.setATIME(29) || !colorSensor.setASTEP(599) ||
      !colorSensor.setGain(AS7341_GAIN_16X)) {
    Serial.println("ERROR: Could not configure the AS7341 exposure.");
    return;
  }

  sensorReady = true;
  Serial.println("AS7341 detected at 0x39. Raw readings:");
  Serial.println("F1_415nm,F2_445nm,F3_480nm,F4_515nm,F5_555nm,F6_590nm,F7_630nm,F8_680nm,Clear,NIR");
}

void loop() {
  if (!sensorReady) {
    delay(1000);
    return;
  }
  if (!readSpectrum()) {
    Serial.println("ERROR: AS7341 sample timed out. Check the I2C connection.");
    delay(1000);
    return;
  }

  const as7341_color_channel_t channels[] = {
      AS7341_CHANNEL_415nm_F1, AS7341_CHANNEL_445nm_F2,
      AS7341_CHANNEL_480nm_F3, AS7341_CHANNEL_515nm_F4,
      AS7341_CHANNEL_555nm_F5, AS7341_CHANNEL_590nm_F6,
      AS7341_CHANNEL_630nm_F7, AS7341_CHANNEL_680nm_F8,
      AS7341_CHANNEL_CLEAR,   AS7341_CHANNEL_NIR,
  };
  for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i) {
    if (i != 0) Serial.print(',');
    Serial.print(colorSensor.getChannel(channels[i]));
  }
  Serial.println();
  delay(400);
}
