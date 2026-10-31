#include <Arduino.h>
#include <Wire.h>

// ESP32-WROOM-32 DevKit: change only if your I2C wiring differs.
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;

constexpr uint8_t TCA_ADDRESS = 0x70;   // A0/A1/A2 tied LOW
constexpr uint8_t AS5600_ADDRESS = 0x36;
constexpr uint8_t LEFT_ENCODER_CHANNEL = 5;
constexpr uint8_t RIGHT_ENCODER_CHANNEL = 4;
constexpr uint32_t SAMPLE_MS = 100;

struct EncoderState {
  const char *name;
  uint8_t channel;
  int16_t previousRaw;
  uint16_t zeroRaw;
  int32_t totalCounts;
  bool tared;
};

EncoderState leftEncoder = {"LEFT", LEFT_ENCODER_CHANNEL, -1, 0, 0, false};
EncoderState rightEncoder = {"RIGHT", RIGHT_ENCODER_CHANNEL, -1, 0, 0, false};
uint32_t previousSampleMs = 0;

bool selectChannel(uint8_t channel) {
  if (channel > 7) return false;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(uint8_t(1U << channel));
  return Wire.endTransmission() == 0;
}

bool devicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t startRegister, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(startRegister);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(AS5600_ADDRESS, count) != count) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
  return true;
}

bool readEncoder(uint8_t channel, uint16_t &raw, uint8_t &status,
                 uint8_t &agc, uint16_t &magnitude) {
  if (!selectChannel(channel)) return false;

  uint8_t angleBytes[2];
  uint8_t magnitudeBytes[2];
  if (!readRegisters(0x0C, angleBytes, 2)) return false;
  if (!readRegisters(0x0B, &status, 1)) return false;
  if (!readRegisters(0x1A, &agc, 1)) return false;
  if (!readRegisters(0x1B, magnitudeBytes, 2)) return false;

  raw = ((uint16_t(angleBytes[0]) & 0x0F) << 8) | angleBytes[1];
  magnitude = ((uint16_t(magnitudeBytes[0]) & 0x0F) << 8) | magnitudeBytes[1];
  return true;
}

// Same tare behavior as the original MagneticEncoder: current angle becomes
// zero and the accumulated turn count is cleared.
bool tareEncoder(EncoderState &encoder) {
  uint16_t raw, magnitude;
  uint8_t status, agc;
  if (!readEncoder(encoder.channel, raw, status, agc, magnitude)) {
    encoder.tared = false;
    return false;
  }
  encoder.zeroRaw = raw;
  encoder.previousRaw = raw;
  encoder.totalCounts = 0;
  encoder.tared = true;
  return true;
}

void scanMuxForEncoders() {
  Serial.println("AS5600 scan across TCA9548A channels:");
  for (uint8_t channel = 0; channel < 8; ++channel) {
    if (!selectChannel(channel)) {
      Serial.printf("  channel %u: mux error\n", channel);
      continue;
    }
    if (devicePresent(AS5600_ADDRESS)) {
      Serial.printf("  channel %u: AS5600 found at 0x36\n", channel);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  Serial.println("\nESP32 + TCA9548A + dual AS5600 encoder test");
  Serial.printf("SDA=%d SCL=%d, LEFT=channel %u, RIGHT=channel %u\n",
                SDA_PIN, SCL_PIN, LEFT_ENCODER_CHANNEL, RIGHT_ENCODER_CHANNEL);
  Serial.println("Keep motor power/enable OFF. Turn each wheel by hand.");
  Serial.println("Both encoders tare at startup. Send z to tare again.");

  if (!devicePresent(TCA_ADDRESS)) {
    Serial.println("ERROR: TCA9548A not found at 0x70. Check power, GND, SDA, SCL, and A0-A2.");
    return;
  }

  scanMuxForEncoders();
  Serial.println(tareEncoder(leftEncoder) ? "LEFT CH5 tared to zero."
                                          : "ERROR: LEFT CH5 could not tare.");
  Serial.println(tareEncoder(rightEncoder) ? "RIGHT CH4 tared to zero."
                                           : "ERROR: RIGHT CH4 could not tare.");
}

void printEncoder(EncoderState &encoder, uint32_t elapsedMs) {
  if (!encoder.tared && !tareEncoder(encoder)) {
    Serial.printf("%s CH%u READ ERROR: unable to tare.\n",
                  encoder.name, encoder.channel);
    return;
  }
  uint16_t raw, magnitude;
  uint8_t status, agc;
  if (!readEncoder(encoder.channel, raw, status, agc, magnitude)) {
    Serial.printf("%s CH%u READ ERROR: mux or AS5600 did not respond.\n",
                  encoder.name, encoder.channel);
    encoder.previousRaw = -1;
    return;
  }

  int16_t delta = 0;
  if (encoder.previousRaw >= 0) {
    delta = int16_t(raw) - encoder.previousRaw;
    if (delta > 2048) delta -= 4096;
    if (delta < -2048) delta += 4096;
    encoder.totalCounts += delta;
  }
  encoder.previousRaw = raw;

  const char *magnet = "OK";
  if (!(status & 0x20)) magnet = "NOT DETECTED";
  else if (status & 0x10) magnet = "TOO WEAK";
  else if (status & 0x08) magnet = "TOO STRONG";

  uint16_t relativeRaw = (raw + 4096 - encoder.zeroRaw) & 0x0FFF;
  float angleDeg = relativeRaw * (360.0f / 4096.0f);
  float totalDeg = encoder.totalCounts * (360.0f / 4096.0f);
  float rpm = elapsedMs ? (delta * 60000.0f / (4096.0f * elapsedMs)) : 0.0f;

  Serial.printf("%s CH%u raw=%4u delta=%+4d tared_angle=%7.2f deg total=%9.2f deg rpm=%7.2f status=0x%02X agc=%3u mag=%4u magnet=%s\n",
                encoder.name, encoder.channel, raw, delta, angleDeg, totalDeg, rpm,
                status, agc, magnitude, magnet);
}

void loop() {
  while (Serial.available()) {
    char command = Serial.read();
    if (command == 'z' || command == 'Z') {
      Serial.println(tareEncoder(leftEncoder) ? "LEFT CH5 re-tared."
                                              : "ERROR: LEFT CH5 could not re-tare.");
      Serial.println(tareEncoder(rightEncoder) ? "RIGHT CH4 re-tared."
                                               : "ERROR: RIGHT CH4 could not re-tare.");
    }
  }

  uint32_t now = millis();
  if (now - previousSampleMs < SAMPLE_MS) return;
  uint32_t elapsedMs = now - previousSampleMs;
  previousSampleMs = now;

  printEncoder(leftEncoder, elapsedMs);
  printEncoder(rightEncoder, elapsedMs);
}
