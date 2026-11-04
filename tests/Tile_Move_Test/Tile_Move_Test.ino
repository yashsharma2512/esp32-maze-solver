// Standalone ESP32-WROOM-32 one-tile movement test.
// Based on MAZESOLVER/motors.ino MoveStrightToDistance() and as5600.h.
// This sketch does not run the maze solver or the wall-follow test.
#include <Arduino.h>
#include <Wire.h>

constexpr int SDA_PIN = 21, SCL_PIN = 22;
constexpr uint8_t TCA_ADDR = 0x70, AS5600_ADDR = 0x36;
constexpr uint8_t LEFT_CH = 5, RIGHT_CH = 4;
constexpr int LMF = 25, LMR = 26, PWML = 27;
constexpr int RMF = 18, RMR = 19, PWMR = 23, STBY = 33;
constexpr int BUTTON_GPIO = 0; // DevKit BOOT button; press after power-up.

// Robot measurements. Change these if the measured dimensions differ.
constexpr float WHEEL_DIAMETER_MM = 70.0f;
constexpr float TILE_LENGTH_MM = 300.0f;
// Original distance drive: remaining degrees * 1.9, clamped to 60..125.
constexpr float DISTANCE_GAIN = 1.9f;
constexpr int MIN_PWM = 60, MAX_PWM = 125;
constexpr float BALANCE_GAIN = 0.5f; // PWM correction per mm wheel mismatch.
constexpr int MAX_BALANCE_PWM = 20;
constexpr uint32_t SAMPLE_MS = 20, PRINT_MS = 200;
constexpr uint32_t MAX_MOVE_MS = 15000; // Fault timeout if target is never reached.

struct Encoder {
  const char *name;
  uint8_t channel;
  uint16_t raw;
  int16_t previousRaw;
  int32_t totalCounts;
  uint8_t status;
  uint8_t agc;
  uint16_t magnitude;
  bool readOK;
};
Encoder leftEnc = {"LEFT", LEFT_CH, 0, -1, 0, 0, 0, 0, false};
Encoder rightEnc = {"RIGHT", RIGHT_CH, 0, -1, 0, 0, 0, 0, false};

bool busReady = false, moving = false;
int moveDirection = 1, leftPWM = 0, rightPWM = 0;
uint32_t moveStartMs = 0, lastSampleMs = 0, lastPrintMs = 0;
const char *lastStopReason = "none";
const float WHEEL_CIRCUMFERENCE_MM = WHEEL_DIAMETER_MM * PI;
const float TARGET_DEGREES = TILE_LENGTH_MM / WHEEL_CIRCUMFERENCE_MM * 360.0f;

bool ping(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool selectChannel(uint8_t channel) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(uint8_t(1U << channel));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(TCA_ADDR, uint8_t(1)) != 1) return false;
  return Wire.read() == uint8_t(1U << channel);
}

bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(AS5600_ADDR, count) != count) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
  return true;
}

bool updateEncoder(Encoder &e) {
  if (!selectChannel(e.channel)) return e.readOK = false;
  uint8_t angle[2], mag[2];
  if (!readRegisters(0x0C, angle, 2) ||
      !readRegisters(0x0B, &e.status, 1) ||
      !readRegisters(0x1A, &e.agc, 1) ||
      !readRegisters(0x1B, mag, 2)) return e.readOK = false;
  e.raw = ((uint16_t(angle[0]) & 0x0F) << 8) | angle[1];
  e.magnitude = ((uint16_t(mag[0]) & 0x0F) << 8) | mag[1];
  if (e.previousRaw >= 0) {
    int16_t delta = int16_t(e.raw) - e.previousRaw;
    if (delta > 2048) delta -= 4096;
    if (delta < -2048) delta += 4096;
    e.totalCounts += delta;
  }
  e.previousRaw = e.raw;
  return e.readOK = true;
}

bool goodMagnet(const Encoder &e) {
  return (e.status & 0x20) && !(e.status & 0x18);
}

const char *magnetLabel(const Encoder &e) {
  if (!e.readOK) return "READ_ERROR";
  if (!(e.status & 0x20)) return "NOT_DETECTED";
  if (e.status & 0x10) return "TOO_WEAK";
  if (e.status & 0x08) return "TOO_STRONG";
  return "OK";
}

bool readBoth() {
  bool leftOK = updateEncoder(leftEnc);
  bool rightOK = updateEncoder(rightEnc);
  return leftOK && rightOK;
}

float wheelMM(const Encoder &e) {
  return fabsf(float(e.totalCounts)) * WHEEL_CIRCUMFERENCE_MM / 4096.0f;
}

void stopMotors() {
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(STBY, LOW);
  digitalWrite(LMF, LOW);
  digitalWrite(LMR, LOW);
  digitalWrite(RMF, LOW);
  digitalWrite(RMR, LOW);
  leftPWM = rightPWM = 0;
}

void initMotorPins() {
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);
  for (int pin : {LMF, LMR, PWML, RMF, RMR, PWMR}) pinMode(pin, OUTPUT);
  stopMotors();
}

// Same signed motor convention and left trim as the original firmware.
void moveMotor(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);
  left = int(left * 0.985f);
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(LMF, left > 0 ? HIGH : LOW);
  digitalWrite(LMR, left < 0 ? HIGH : LOW);
  digitalWrite(RMF, right > 0 ? HIGH : LOW);
  digitalWrite(RMR, right < 0 ? HIGH : LOW);
  digitalWrite(STBY, HIGH);
  analogWrite(PWML, abs(left));
  analogWrite(PWMR, abs(right));
  leftPWM = left;
  rightPWM = right;
}

void stopMove(const char *reason) {
  stopMotors();
  if (moving) {
    lastStopReason = reason;
    Serial.printf("STOP: %s | left=%.1f mm right=%.1f mm average=%.1f mm\n",
                  reason, wheelMM(leftEnc), wheelMM(rightEnc),
                  (wheelMM(leftEnc) + wheelMM(rightEnc)) * 0.5f);
  }
  moving = false;
}

void startMove(int direction) {
  if (moving || !busReady) return;
  if (!readBoth()) {
    Serial.println("START BLOCKED: encoder I2C read failed.");
    return;
  }
  if (!goodMagnet(leftEnc) || !goodMagnet(rightEnc)) {
    Serial.printf("START BLOCKED: magnet LEFT=%s RIGHT=%s. Correct magnet gap/centering first.\n",
                  magnetLabel(leftEnc), magnetLabel(rightEnc));
    return;
  }
  // Original MoveStrightToDistance() tares before each distance command.
  leftEnc.totalCounts = rightEnc.totalCounts = 0;
  leftEnc.previousRaw = leftEnc.raw;
  rightEnc.previousRaw = rightEnc.raw;
  moveDirection = direction;
  moveStartMs = millis();
  lastStopReason = "none";
  moving = true;
  Serial.printf("START %s one tile: %.1f mm = %.1f wheel degrees\n",
                direction > 0 ? "FORWARD" : "REVERSE", TILE_LENGTH_MM,
                TARGET_DEGREES);
}

void printStatus() {
  float lmm = wheelMM(leftEnc), rmm = wheelMM(rightEnc);
  Serial.printf("%s L=%.1f mm (%s) R=%.1f mm (%s) avg=%.1f/%.1f mm PWM=%d,%d last_stop=%s\n",
                moving ? "MOVE" : "IDLE", lmm, magnetLabel(leftEnc),
                rmm, magnetLabel(rightEnc), (lmm + rmm) * 0.5f,
                TILE_LENGTH_MM, leftPWM, rightPWM, lastStopReason);
}

void setup() {
  Serial.begin(115200);
  initMotorPins(); // Driver remains in standby until a tile command.
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  delay(500);
  Serial.println("\nESP32 ONE-TILE MOTOR + AS5600 TEST");
  Serial.printf("Wheel=%.1f mm, tile=%.1f mm, encoder channels LEFT=%u RIGHT=%u\n",
                WHEEL_DIAMETER_MM, TILE_LENGTH_MM, LEFT_CH, RIGHT_CH);
  Serial.println("BOOT or g: forward one tile | b: reverse one tile | x: stop");
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(50);
  busReady = ping(TCA_ADDR) && selectChannel(LEFT_CH) && ping(AS5600_ADDR) &&
             selectChannel(RIGHT_CH) && ping(AS5600_ADDR);
  if (!busReady) Serial.println("ERROR: TCA or encoder missing. Motor start disabled.");
  else if (readBoth()) printStatus();
}

void loop() {
  static bool wasBootPressed = false;
  static uint32_t lastBootMs = 0;
  bool pressed = digitalRead(BUTTON_GPIO) == LOW;
  if (pressed && !wasBootPressed && millis() - lastBootMs > 250) {
    lastBootMs = millis();
    if (moving) stopMove("BOOT button");
    else startMove(1);
  }
  wasBootPressed = pressed;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'x' || c == 'X') stopMove("user command");
    else if (c == 'g' || c == 'G') startMove(1);
    else if (c == 'b' || c == 'B') startMove(-1);
  }

  uint32_t now = millis();
  if (moving && now - moveStartMs > MAX_MOVE_MS) stopMove("encoder timeout");
  if (busReady && now - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = now;
    if (!readBoth()) {
      if (moving) stopMove("encoder I2C error");
    } else if (moving) {
      if (!goodMagnet(leftEnc) || !goodMagnet(rightEnc)) {
        stopMove("magnet status changed");
      } else {
        float lmm = wheelMM(leftEnc), rmm = wheelMM(rightEnc);
        float avgMM = (lmm + rmm) * 0.5f;
        if (avgMM >= TILE_LENGTH_MM) {
          stopMove("tile reached");
        } else if (now - moveStartMs > 750 && (lmm < 3.0f || rmm < 3.0f)) {
          stopMove("wheel or encoder stalled");
        } else if (avgMM > 50.0f && fabsf(lmm - rmm) > 60.0f) {
          stopMove("wheel distance mismatch");
        } else {
          float remainingDegrees = (TILE_LENGTH_MM - avgMM) /
                                   WHEEL_CIRCUMFERENCE_MM * 360.0f;
          int drive = constrain(int(remainingDegrees * DISTANCE_GAIN),
                                MIN_PWM, MAX_PWM);
          int balance = constrain(int((lmm - rmm) * BALANCE_GAIN),
                                  -MAX_BALANCE_PWM, MAX_BALANCE_PWM);
          moveMotor(moveDirection * (drive - balance),
                    moveDirection * (drive + balance));
        }
      }
    }
  }
  if (now - lastPrintMs >= PRINT_MS) {
    lastPrintMs = now;
    printStatus();
  }
  delay(1);
}
