// Continuous left-hand maze run for the ESP32-WROOM-32 robot.
#include "Hardware.h"

constexpr int START_BUTTON_PIN = 0;
constexpr uint32_t CONTROL_MS = 20, MAX_CONTROL_GAP_MS = 120;
constexpr uint32_t PRINT_MS = 250, TURN_SETTLE_MS = 150;
constexpr uint8_t GESTURE_HIGH = 150, GESTURE_RELEASE = 100;
constexpr uint8_t SIDE_OPEN = 9, SIDE_PRESENT = 25, FRONT_BLOCKED = 40;
constexpr float WALL_TARGET = 50.0f, KP = 0.85f, KD = 1.5f;
constexpr int BASE_PWM = 70, MAX_STEERING = 25;
constexpr int MIN_TURN_PWM = 55, MAX_TURN_PWM = 100;
constexpr float WHEEL_DIAMETER_MM = 70.0f, CORNER_ADVANCE_MM = 220.0f;
constexpr uint32_t CORNER_TIMEOUT_MS = 4500;

enum class Motion { Idle, Follow, CornerAdvance, Turn, Settle };
enum class Gesture { WaitClear, LeftHand, RightHand, Release };
Motion motion = Motion::Idle;
Gesture gesture = Gesture::WaitClear;
bool busReady = false, apdsReady = false, imuReady = false;
bool startRequested = false, leftWallSeen = false;
const char *lastStop = "none";
uint32_t lastControl = 0, lastPrint = 0, phaseStarted = 0;
int32_t cornerLeftStart = 0, cornerRightStart = 0;
float previousError = 0, turnStartYaw = 0, turnDegrees = 0;
int turnDirection = -1; // Left = -1, right = 1.

const char *motionName() {
  switch (motion) {
    case Motion::Idle: return "IDLE";
    case Motion::Follow: return "FOLLOW";
    case Motion::CornerAdvance: return "CORNER_ADVANCE";
    case Motion::Turn: return "TURN";
    case Motion::Settle: return "SETTLE";
  }
  return "UNKNOWN";
}

void stopRun(const char *reason) {
  stopMotors();
  motion = Motion::Idle;
  gesture = Gesture::WaitClear;
  startRequested = false;
  lastStop = reason;
  Serial.printf("STOP: %s\n", reason);
}

void serviceControls() {
  static bool previousBoot = digitalRead(START_BUTTON_PIN) == LOW;
  static uint32_t lastBootAction = 0;
  const bool pressed = digitalRead(START_BUTTON_PIN) == LOW;
  if (pressed && !previousBoot && millis() - lastBootAction >= 250) {
    lastBootAction = millis();
    if (motion == Motion::Idle) startRequested = true;
    else stopRun("BOOT button");
  }
  previousBoot = pressed;

  while (Serial.available()) {
    const int command = Serial.read();
    if (command == 'x' || command == 'X') stopRun("serial command");
    if ((command == 's' || command == 'S') && motion == Motion::Idle) {
      startRequested = true;
    }
  }
}

void updateGesture() {
  switch (gesture) {
    case Gesture::WaitClear:
      if (leftAPDS.raw < GESTURE_RELEASE && rightAPDS.raw < GESTURE_RELEASE) {
        gesture = Gesture::LeftHand;
        Serial.println("Show a hand to the LEFT APDS (CH0), then move it away.");
      }
      break;
    case Gesture::LeftHand:
      if (leftAPDS.raw > GESTURE_HIGH) {
        gesture = Gesture::RightHand;
        Serial.println("Show a hand to the RIGHT APDS (CH1), then move it away.");
      }
      break;
    case Gesture::RightHand:
      if (leftAPDS.raw < GESTURE_RELEASE && rightAPDS.raw > GESTURE_HIGH) {
        gesture = Gesture::Release;
      }
      break;
    case Gesture::Release:
      if (leftAPDS.raw < GESTURE_RELEASE && rightAPDS.raw < GESTURE_RELEASE &&
          !leftAPDS.saturated && !rightAPDS.saturated && !frontAPDS.saturated) {
        startRequested = true;
        gesture = Gesture::WaitClear;
      }
      break;
  }
}

void beginRun() {
  leftWallSeen = leftAPDS.filtered >= SIDE_PRESENT;
  previousError = 0;
  lastStop = "none";
  motion = Motion::Follow;
  Serial.println("START: continuous left-wall following. Send x or press BOOT to stop.");
}

void beginTurn(int direction, float degrees) {
  stopMotors();
  turnDirection = direction;
  turnDegrees = degrees;
  turnStartYaw = yaw;
  phaseStarted = millis();
  motion = Motion::Turn;
  Serial.printf("TURN: %s %.0f degrees\n", direction < 0 ? "LEFT" : "RIGHT", degrees);
}

void beginCorner() {
  if (!encoderUsable(leftEncoder) || !encoderUsable(rightEncoder)) {
    stopRun("left opening found, but corner clearance needs both encoders");
    return;
  }
  cornerLeftStart = leftEncoder.counts;
  cornerRightStart = rightEncoder.counts;
  phaseStarted = millis();
  motion = Motion::CornerAdvance;
  Serial.println("LEFT opening: advance to clear the corner.");
}

bool frontBlocked() {
  return frontAPDS.raw >= FRONT_BLOCKED || frontAPDS.filtered >= FRONT_BLOCKED;
}

void followWalls() {
  if (frontBlocked()) {
    // Prefer left, then right, then a U-turn when both sides are closed.
    if (leftAPDS.filtered <= SIDE_OPEN) beginTurn(-1, 90);
    else if (rightAPDS.filtered <= SIDE_OPEN) beginTurn(1, 90);
    else beginTurn(-1, 180);
    return;
  }
  if (leftWallSeen && leftAPDS.filtered <= SIDE_OPEN) {
    beginCorner();
    return;
  }

  const bool leftWall = leftAPDS.filtered >= SIDE_PRESENT;
  const bool rightWall = rightAPDS.filtered >= SIDE_PRESENT;
  if (leftWall) leftWallSeen = true;

  // Original straight-line P+D control for two walls, one wall, or open space.
  float error = 0;
  if (leftWall && rightWall) error = leftAPDS.filtered - rightAPDS.filtered;
  else if (leftWall) error = leftAPDS.filtered - WALL_TARGET;
  else if (rightWall) error = WALL_TARGET - rightAPDS.filtered;
  const int steering = int(constrain(KP * error + KD * (error - previousError),
                                    -float(MAX_STEERING), float(MAX_STEERING)));
  previousError = error;
  moveMotor(BASE_PWM + steering, BASE_PWM - steering);
}

void advanceAtCorner(uint32_t now) {
  if (!encoderUsable(leftEncoder) || !encoderUsable(rightEncoder)) {
    stopRun("encoder read or magnet fault during corner clearance");
    return;
  }
  const float left = fabsf(float(leftEncoder.counts - cornerLeftStart));
  const float right = fabsf(float(rightEncoder.counts - cornerRightStart));
  const float distanceMM = (left + right) * 0.5f * PI * WHEEL_DIAMETER_MM / 4096.0f;
  if (frontBlocked() || distanceMM >= CORNER_ADVANCE_MM) {
    beginTurn(-1, 90);
  } else if (now - phaseStarted > CORNER_TIMEOUT_MS) {
    stopRun("corner clearance timed out");
  } else {
    moveMotor(BASE_PWM, BASE_PWM);
  }
}

void updateTurn(uint32_t now) {
  const float turned = fabsf(yaw - turnStartYaw);
  const float remaining = turnDegrees - turned;
  const bool reachedTarget = remaining <= 3.0f;
  const uint32_t turnTimeout = turnDegrees > 90.0f ? 5000UL : 3000UL;
  if (reachedTarget || now - phaseStarted >= turnTimeout) {
    stopMotors();
    phaseStarted = now;
    motion = Motion::Settle;
    if (!reachedTarget) {
      Serial.printf("TURN TIMEOUT: %.1f of %.0f degrees. Rechecking walls.\n", turned, turnDegrees);
    }
    return;
  }
  const int speed = constrain(int(2.0f * remaining), MIN_TURN_PWM, MAX_TURN_PWM);
  moveMotor(turnDirection * speed, -turnDirection * speed);
}

void controlStep(uint32_t now) {
  if (motion != Motion::Idle && now - lastControl > MAX_CONTROL_GAP_MS) {
    stopRun("navigation loop ran late");
  }
  lastControl = now;
  if (!busReady) return;

  const bool leftOK = apdsReady && sampleAPDS(leftAPDS);
  const bool rightOK = apdsReady && sampleAPDS(rightAPDS);
  const bool frontOK = apdsReady && sampleAPDS(frontAPDS);
  const bool gyroOK = imuReady && sampleIMU();
  sampleEncoder(leftEncoder);
  sampleEncoder(rightEncoder);
  const bool samplesOK = leftOK && rightOK && frontOK && gyroOK;
  const bool saturated = leftAPDS.saturated || rightAPDS.saturated || frontAPDS.saturated;

  if (motion == Motion::Idle) {
    if (samplesOK && !startRequested) updateGesture();
    if (startRequested) {
      startRequested = false;
      if (samplesOK && !saturated) beginRun();
      else Serial.println("START BLOCKED: check the APDS readings and IMU.");
    }
    return;
  }

  if (!samplesOK || saturated || millis() - now > MAX_CONTROL_GAP_MS) {
    stopRun(saturated ? "APDS proximity saturated" : "navigation sensor unavailable or late");
    return;
  }

  switch (motion) {
    case Motion::Follow: followWalls(); break;
    case Motion::CornerAdvance: advanceAtCorner(now); break;
    case Motion::Turn: updateTurn(now); break;
    case Motion::Settle:
      if (now - phaseStarted >= TURN_SETTLE_MS) {
        previousError = 0;
        leftWallSeen = leftAPDS.filtered >= SIDE_PRESENT;
        motion = Motion::Follow;
      }
      break;
    case Motion::Idle: break;
  }
}

void printStatus() {
  Serial.printf("%s APDS L=%u R=%u F=%u | yaw=%+.1f | enc L=%ld(%s) R=%ld(%s) | PWM %d/%d | last_stop=%s\n",
                motionName(), unsigned(leftAPDS.raw), unsigned(rightAPDS.raw),
                unsigned(frontAPDS.raw), yaw, long(leftEncoder.counts),
                encoderUsable(leftEncoder) ? "OK" : "CHECK", long(rightEncoder.counts),
                encoderUsable(rightEncoder) ? "OK" : "CHECK", leftPWM, rightPWM, lastStop);
}

void printColorSample() {
  const as7341_color_channel_t channels[] = {
      AS7341_CHANNEL_415nm_F1, AS7341_CHANNEL_445nm_F2, AS7341_CHANNEL_480nm_F3,
      AS7341_CHANNEL_515nm_F4, AS7341_CHANNEL_555nm_F5, AS7341_CHANNEL_590nm_F6,
      AS7341_CHANNEL_630nm_F7, AS7341_CHANNEL_680nm_F8,
      AS7341_CHANNEL_CLEAR, AS7341_CHANNEL_NIR,
  };
  Serial.print("AS7341 F1..F8,Clear,NIR:");
  for (auto channel : channels) Serial.printf(" %u", unsigned(colorSensor.getChannel(channel)));
  Serial.println();
}

void setup() {
  setMotorIOs();
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  delay(750);
  Serial.println("\nMAZESOLVER | ESP32 Dev Module | continuous left-wall following");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("ERROR: could not start I2C.");
    return;
  }
  Wire.setTimeOut(20);
  if (!selectMux(0)) {
    Serial.println("ERROR: TCA9548A at 0x70 did not respond.");
    return;
  }
  Wire.beginTransmission(APDS_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("ERROR: 0x39 is on the main bus. Put the AS7341 on TCA CH2.");
    return;
  }
  busReady = true;

  apdsReady = true;
  for (const Proximity *sensor : {&leftAPDS, &rightAPDS, &frontAPDS}) {
    const bool ok = initAPDS(*sensor);
    apdsReady = apdsReady && ok;
    Serial.printf("APDS %s CH%u: %s\n", sensor->name, unsigned(sensor->channel),
                  ok ? "READY" : "INIT ERROR");
  }
  imuReady = selectMux(0) && initIMU();
  Serial.println(imuReady ? "Gyro calibrated." : "IMU init/calibration failed. Keep still and reset.");
  colorReady = initColor();
  Serial.println(colorReady ? "AS7341 CH2 ready. Color samples print while stopped."
                            : "AS7341 CH2 init failed. Check SD2/SC2.");
  Serial.println("Start with LEFT then RIGHT hand gestures, BOOT, or serial s. Stop with BOOT or x.");
}

void loop() {
  serviceControls();
  const uint32_t now = millis();
  if (now - lastControl >= CONTROL_MS) controlStep(now);
  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    printStatus();
  }
  if (motion == Motion::Idle && colorReady && now - lastColorSample >= 600) {
    const bool colorSampleOK = sampleColor();
    lastColorSample = millis();
    if (colorSampleOK) printColorSample();
    else Serial.println("AS7341 CH2 read timed out or channel selection failed.");
  }
  delay(1);
}
