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
constexpr uint8_t ATIME = 29;
constexpr uint16_t ASTEP = 599;
constexpr uint32_t INTEGRATION_COUNTS = uint32_t(ATIME + 1) * (ASTEP + 1);
constexpr uint16_t FULL_SCALE = INTEGRATION_COUNTS < UINT16_MAX ? INTEGRATION_COUNTS : UINT16_MAX;
constexpr uint32_t SAMPLE_TIMEOUT_MS = 1000;
constexpr uint8_t REFERENCE_SAMPLES = 12;
constexpr int MIN_REFERENCE_SPAN = 20;

constexpr as7341_color_channel_t channels[] = {
    AS7341_CHANNEL_415nm_F1, AS7341_CHANNEL_445nm_F2,
    AS7341_CHANNEL_480nm_F3, AS7341_CHANNEL_515nm_F4,
    AS7341_CHANNEL_555nm_F5, AS7341_CHANNEL_590nm_F6,
    AS7341_CHANNEL_630nm_F7, AS7341_CHANNEL_680nm_F8,
    AS7341_CHANNEL_CLEAR, AS7341_CHANNEL_NIR,
};
constexpr size_t CHANNEL_COUNT = sizeof(channels) / sizeof(channels[0]);

Adafruit_AS7341 colorSensor;
bool sensorReady = false;
uint16_t blackReference[CHANNEL_COUNT] = {}, whiteReference[CHANNEL_COUNT] = {};
bool haveBlack = false, haveWhite = false;

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

void captureReference(const char *name, uint16_t *reference, bool &captured) {
  Serial.printf("Hold the %s target still. Averaging %u samples.\n",
                name, unsigned(REFERENCE_SAMPLES));
  uint32_t totals[CHANNEL_COUNT] = {};
  for (uint8_t sample = 0; sample < REFERENCE_SAMPLES; ++sample) {
    if (!readSpectrum()) {
      Serial.println("Reference capture failed: sensor read timed out.");
      return;
    }
    for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
      const uint16_t raw = colorSensor.getChannel(channels[i]);
      if (raw >= FULL_SCALE) {
        Serial.println("Reference capture failed: a band clipped. Reduce light or exposure.");
        return;
      }
      totals[i] += raw;
    }
  }

  captured = true;
  Serial.printf("%s reference", name);
  for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
    reference[i] = (totals[i] + REFERENCE_SAMPLES / 2) / REFERENCE_SAMPLES;
    Serial.printf(",%u", unsigned(reference[i]));
  }
  Serial.println();
}

void handleSerial() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'b': case 'B': captureReference("Black", blackReference, haveBlack); break;
      case 'w': case 'W': captureReference("White", whiteReference, haveWhite); break;
      case 'r': case 'R':
        haveBlack = haveWhite = false;
        Serial.println("References cleared.");
        break;
    }
  }
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
  if (!colorSensor.setATIME(ATIME) || !colorSensor.setASTEP(ASTEP) ||
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
  Serial.println("Calibration commands: b = capture black, w = capture white, r = clear.");
  Serial.println("AS7341 detected at 0x39. Raw readings:");
  Serial.println("F1_415nm,F2_445nm,F3_480nm,F4_515nm,F5_555nm,F6_590nm,F7_630nm,F8_680nm,Clear,NIR");
}

void loop() {
  if (!sensorReady) {
    delay(1000);
    return;
  }
  handleSerial();
  if (!readSpectrum()) {
    Serial.println("ERROR: AS7341 sample timed out. Check the I2C connection.");
    delay(1000);
    return;
  }

  for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
    if (i != 0) Serial.print(',');
    Serial.print(colorSensor.getChannel(channels[i]));
  }
  Serial.println();
  if (haveBlack && haveWhite) {
    Serial.print("REL_PCT");
    for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
      const int span = int(whiteReference[i]) - int(blackReference[i]);
      const uint16_t raw = colorSensor.getChannel(channels[i]);
      if (span < MIN_REFERENCE_SPAN || raw >= FULL_SCALE) Serial.print(",NA");
      else Serial.printf(",%.1f", 100.0f * (int(raw) - int(blackReference[i])) / span);
    }
    Serial.println();
  }
  delay(400);
}
