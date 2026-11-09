// ESP32 Dev Module. Install Adafruit AS7341 and Adafruit BusIO.
// TCA9548A on GPIO21 SDA / GPIO22 SCL. AS7341 on SD2 / SC2.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS7341.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr uint8_t TCA_ADDRESS = 0x70;
constexpr uint8_t AS7341_TCA_CHANNEL = 2;
constexpr uint16_t LED_CURRENT_MA = 4;
constexpr uint32_t SAMPLE_TIMEOUT_MS = 1000;

Adafruit_AS7341 colorSensor;
bool sensorReady = false;

// Write one channel bit so the AS7341 is the only 0x39 sensor on the bus.
bool selectColorSensorChannel() {
  constexpr uint8_t channelMask = 1U << AS7341_TCA_CHANNEL;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(channelMask);
  const uint8_t result = Wire.endTransmission();
  if (result != 0) {
    Serial.printf("ERROR: TCA at 0x70 did not accept the channel selection (I2C error %u).\n",
                  unsigned(result));
    return false;
  }
  if (Wire.requestFrom(TCA_ADDRESS, uint8_t(1)) != 1 ||
      Wire.read() != channelMask) {
    Serial.println("ERROR: TCA channel selection could not be verified.");
    return false;
  }
  Serial.printf("TCA channel %u selected.\n", unsigned(AS7341_TCA_CHANNEL));
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
  Serial.println("\nESP32 AS7341 test through TCA9548A");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("ERROR: Could not start I2C.");
    return;
  }
  Wire.setTimeOut(50);
  if (!selectColorSensorChannel()) return;

  if (!colorSensor.begin(AS7341_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("ERROR: AS7341 at 0x39 did not respond with the expected ID.");
    Serial.println("Check AS7341 power, GND, and SDA/SCL on TCA SD2/SC2.");
    return;
  }
  if (!colorSensor.setATIME(29) || !colorSensor.setASTEP(599) ||
      !colorSensor.setGain(AS7341_GAIN_16X)) {
    Serial.println("ERROR: Could not configure the AS7341 exposure.");
    return;
  }

  if (colorSensor.setLEDCurrent(LED_CURRENT_MA) && colorSensor.enableLED(true)) {
    Serial.printf("AS7341 illumination LED enabled at %u mA.\n", unsigned(LED_CURRENT_MA));
  } else {
    Serial.println("WARNING: Could not enable the AS7341 illumination LED.");
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
