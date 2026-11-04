// ESP32-WROOM-32: short LEFT wall tracking test using APDS-9960 on TCA CH0.
// Derived from MAZESOLVER/proximity.ino, motors.ino moveStraightAligned(),
// and the repository's ESP32 motor pin map. No automatic movement.
#include <Arduino.h>
#include <Wire.h>

constexpr int SDA_PIN = 21, SCL_PIN = 22;
constexpr uint8_t TCA_ADDR = 0x70, APDS_ADDR = 0x39, APDS_CH = 0;
constexpr int LMF = 25, LMR = 26, PWML = 27;
constexpr int RMF = 18, RMR = 19, PWMR = 23, STBY = 33;
constexpr int BUTTON_GPIO = 0; // Onboard BOOT button, press only after power-up.

// Original control shape: filtered left value - target, then P + D.
constexpr float FILTER_ALPHA = 0.65f; // Original proximity smoothing.
float targetReading = 50.0f; // Captured at start; original leftWallOffset was 50.
constexpr float MIN_WALL_READING = 25.0f; // Original wallSense.
constexpr float KP = 0.85f, KD = 1.5f; // Original config_lolins2 gains.
// First movement test uses a slower base and smaller steering correction.
constexpr int BASE_PWM = 70;
constexpr int MAX_STEERING = 25;
constexpr uint32_t CONTROL_MS = 20;
constexpr uint32_t PRINT_MS = 100;
constexpr uint32_t MAX_RUN_MS = 2000;

bool apdsReady = false, sampleValid = false, haveFilter = false, running = false;
uint8_t raw = 0, statusReg = 0;
float filtered = 0.0f, previousError = 0.0f, errorNow = 0.0f;
int leftCommand = 0, rightCommand = 0;
uint8_t lowWallSamples = 0;
uint32_t lastControlMs = 0, lastPrintMs = 0, lastValidMs = 0, runStartMs = 0;

bool ping(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool selectChannel0() {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(uint8_t(1U << APDS_CH));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(TCA_ADDR, uint8_t(1)) != 1) return false;
  return Wire.read() == uint8_t(1U << APDS_CH);
}

bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(APDS_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(APDS_ADDR, uint8_t(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// PGSAT latches until PICLEAR (0xE5) or PEN is disabled.
bool clearProximityFlag() {
  Wire.beginTransmission(APDS_ADDR);
  Wire.write(uint8_t(0xE5));
  return Wire.endTransmission() == 0;
}

bool initAPDS() {
  if (!ping(TCA_ADDR) || !selectChannel0() || !ping(APDS_ADDR)) return false;
  uint8_t id = 0;
  if (!readRegister(0x92, id) || id != 0xAB) {
    Serial.printf("APDS ID error: 0x%02X (expected 0xAB)\n", id);
    return false;
  }
  // Same setup as the already-tested channel-0 diagnostic sketch.
  bool ok = writeRegister(0x80, 0x00); // Power off while configuring.
  ok = writeRegister(0x90, 0x31) && ok; // Original LED boost.
  ok = writeRegister(0x8F, 0x49) && ok; // Original gain/drive.
  ok = writeRegister(0x8E, 0x87) && ok; // Eight 8-us IR pulses.
  ok = writeRegister(0x80, 0x01) && ok; // PON.
  delay(10);
  ok = writeRegister(0x80, 0x05) && ok; // PON + PEN.
  delay(50);
  uint8_t enable = 0, pulse = 0;
  ok = readRegister(0x80, enable) && ok;
  ok = readRegister(0x8E, pulse) && ok;
  Serial.printf("APDS CH0 ID=0x%02X ENABLE=0x%02X PPULSE=0x%02X\n",
                id, enable, pulse);
  return ok && enable == 0x05 && pulse == 0x87;
}

bool updateAPDS() {
  if (!selectChannel0()) return false;
  if (!readRegister(0x93, statusReg) || !readRegister(0x9C, raw)) return false;
  bool converted = (statusReg & 0x02) != 0;
  bool saturated = (statusReg & 0x40) != 0;
  // Keep telemetry current even if a saturated sample is unsafe for steering.
  if (converted) {
    filtered = haveFilter ? FILTER_ALPHA * filtered +
                            (1.0f - FILTER_ALPHA) * raw : raw;
    haveFilter = true;
    lastValidMs = millis();
  }
  // Clear the sticky flag so the next cycle can reveal whether saturation
  // is still happening at the current wall distance.
  if (saturated && !clearProximityFlag()) return false;
  return converted && !saturated;
}

void stopMotors() {
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(STBY, LOW);
  digitalWrite(LMF, LOW);
  digitalWrite(LMR, LOW);
  digitalWrite(RMF, LOW);
  digitalWrite(RMR, LOW);
  leftCommand = rightCommand = 0;
}

void initMotorPins() {
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);
  for (int pin : {LMF, LMR, PWML, RMF, RMR, PWMR}) pinMode(pin, OUTPUT);
  stopMotors();
}

// Original moveMotor convention: positive PWM = forward.
// The same 0.985 left trim appears in the maze sketch and repo motor test.
void moveMotor(int left, int right) {
  left = constrain(left, 0, 255);
  right = constrain(right, 0, 255);
  left = int(left * 0.985f);
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(LMF, left > 0 ? HIGH : LOW);
  digitalWrite(LMR, LOW);
  digitalWrite(RMF, right > 0 ? HIGH : LOW);
  digitalWrite(RMR, LOW);
  digitalWrite(STBY, HIGH);
  analogWrite(PWML, left);
  analogWrite(PWMR, right);
  leftCommand = left;
  rightCommand = right;
}

void stopRun(const char *reason) {
  stopMotors();
  if (running) Serial.printf("STOP: %s\n", reason);
  running = false;
  previousError = 0.0f;
}

void startRun() {
  if (running) return;
  if (!apdsReady || !haveFilter || millis() - lastValidMs > 100) {
    Serial.println("START BLOCKED: APDS CH0 has no fresh converted reading.");
    return;
  }
  if (!sampleValid) {
    Serial.println((statusReg & 0x40)
        ? "START BLOCKED: saturation flag was cleared; wait for the next sample."
        : "START BLOCKED: APDS CH0 read failed or conversion is invalid.");
    return;
  }
  if (filtered < MIN_WALL_READING) {
    Serial.printf("START BLOCKED: left wall reading %.1f < %.1f. Check VL/PS and sensor direction.\n",
                  filtered, MIN_WALL_READING);
    return;
  }
  targetReading = filtered; // Hold the distance chosen when the test starts.
  previousError = 0.0f;
  errorNow = 0.0f;
  lowWallSamples = 0;
  runStartMs = millis();
  running = true;
  Serial.printf("START: 2 s max, target=%.1f, base PWM=%d\n",
                targetReading, BASE_PWM);
}

void setup() {
  Serial.begin(115200);
  initMotorPins(); // STBY remains LOW until the user sends 's'.
  delay(500);
  Serial.println("\nLEFT WALL TRACK TEST | ESP32 | APDS on TCA CH0");
  Serial.println("Place robot at the desired distance from a straight LEFT wall.");
  Serial.println("Press BOOT after startup or send s for a 2-second run; press BOOT again or send x to stop.");
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(30);
  apdsReady = initAPDS();
  Serial.printf("APDS CH0: %s\n", apdsReady ? "READY" : "NOT READY");
  if (!apdsReady) Serial.println("Motor start will be blocked.");
}

void loop() {
  static bool wasBootPressed = false;
  static uint32_t lastBootActionMs = 0;
  bool bootPressed = digitalRead(BUTTON_GPIO) == LOW;
  if (bootPressed && !wasBootPressed && millis() - lastBootActionMs > 250) {
    lastBootActionMs = millis();
    if (running) stopRun("BOOT button");
    else startRun();
  }
  wasBootPressed = bootPressed;

  while (Serial.available()) {
    char command = Serial.read();
    if (command == 'x' || command == 'X') stopRun("user command");
    else if (command == 's' || command == 'S') startRun();
  }

  uint32_t now = millis();
  if (running && now - runStartMs >= MAX_RUN_MS) stopRun("2-second limit");

  if (now - lastControlMs >= CONTROL_MS) {
    lastControlMs = now;
    sampleValid = apdsReady && updateAPDS();
    if (running) {
      if (!sampleValid) {
        stopRun("APDS read invalid, saturated, or I2C error");
      } else if (filtered < MIN_WALL_READING) {
        if (++lowWallSamples >= 3) stopRun("left wall lost");
      } else {
        lowWallSamples = 0;
      }
      if (running) {
        // Original left-wall branch of moveStraightAligned():
        // error = left reading - left target; drive = KP*error + KD*(error-prev).
        errorNow = filtered - targetReading;
        float change = errorNow - previousError;
        previousError = errorNow;
        float drive = constrain(KP * errorNow + KD * change,
                                -float(MAX_STEERING), float(MAX_STEERING));
        // Too close (large reading): left motor faster, turn right.
        moveMotor(BASE_PWM + int(drive), BASE_PWM - int(drive));
      }
    }
  }

  if (now - lastPrintMs >= PRINT_MS) {
    lastPrintMs = now;
    Serial.printf("%s CH0 raw=%3u filtered=%6.1f valid=%u saturated=%u usable=%u error=%+6.1f L=%3d R=%3d\n",
                  running ? "RUN " : "IDLE", raw, filtered,
                  !!(statusReg & 0x02), !!(statusReg & 0x40), sampleValid,
                  errorNow, leftCommand, rightCommand);
  }
  delay(1);
}
