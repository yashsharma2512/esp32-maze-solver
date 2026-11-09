// Continuous left-hand maze run for the ESP32-WROOM-32 robot.
#include "Hardware.h"

constexpr int START_BUTTON_PIN = 0;
constexpr uint32_t CONTROL_MS = 10, MAX_CONTROL_GAP_MS = 120;
constexpr uint32_t PRINT_MS = 250;
constexpr uint8_t GESTURE_HIGH = 150, GESTURE_RELEASE = 100;
constexpr uint8_t SIDE_OPEN = 9, SIDE_PRESENT = 25, FRONT_BLOCKED = 40;
constexpr int WALL_TARGET = 50, MAX_WALL_ERROR = 75;
constexpr float KP = 0.85f, KD = 1.5f, GYRO_ASSIST = 1.5f;
constexpr int GYRO_Z_SIGN = 1; // Use -1 if the IMU is mounted upside down.
constexpr int BASE_PWM = 100, MAX_STEERING = 120;
constexpr float TURN_KP = 12.5f, TURN_KD = 0.2f;
constexpr int MIN_TURN_PWM = 55, MAX_TURN_PWM = 165;
constexpr uint32_t TURN_TIMEOUT_MS = 1100, TURN_TARGET_HOLD_MS = 200;
constexpr float WHEEL_DIAMETER_MM = 70.0f;
constexpr float CORNER_ADVANCE_MM = 220.0f, TURN_REVERSE_MM = 30.0f;
constexpr uint32_t DISTANCE_TIMEOUT_MS = 4500, DEAD_END_REVERSE_MS = 1000;

enum class Motion { Idle, Follow, DistanceMove, Turn, Pause, DeadEndReverse };
enum class Step { Follow, ChooseFrontTurn, LeftAtOpening, BackUp, SecondLeft, DeadEndReverse };
enum class Gesture { WaitClear, LeftHand, RightHand, Release };
Motion motion = Motion::Idle;
Step afterPause = Step::Follow, afterTurn = Step::BackUp;
Gesture gesture = Gesture::WaitClear;
bool busReady = false, apdsReady = false, imuReady = false;
bool startRequested = false, leftWallSeen = false;
const char *lastStop = "none";
uint32_t lastControl = 0, lastPrint = 0, phaseStarted = 0, pauseLength = 0;
int32_t distanceLeftStart = 0, distanceRightStart = 0;
float distanceTarget = 0, headingYaw = 0, turnStartYaw = 0;
int previousError = 0, previousTurnError = 0;
bool turnReachedTarget = false;
uint32_t turnTargetAt = 0;
int turnDirection = -1; // Left = -1, right = 1.

const char *motionName() {
  switch (motion) {
    case Motion::Idle: return "IDLE";
    case Motion::Follow: return "FOLLOW";
    case Motion::DistanceMove: return distanceTarget > 0 ? "CORNER_ADVANCE" : "CORNER_REVERSE";
    case Motion::Turn: return "TURN";
    case Motion::Pause: return "PAUSE";
    case Motion::DeadEndReverse: return "DEAD_END_REVERSE";
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
      if (leftAPDS.raw < GESTURE_RELEASE && rightAPDS.raw < GESTURE_RELEASE) {
        startRequested = true;
        gesture = Gesture::WaitClear;
      }
      break;
  }
}

void beginRun() {
  leftWallSeen = leftAPDS.filtered > SIDE_OPEN;
  previousError = 0;
  headingYaw = yaw;
  lastStop = "none";
  motion = Motion::Follow;
  Serial.println("START: continuous left-wall following. Send x or press BOOT to stop.");
}

void beginPause(Step next, uint32_t duration, uint32_t now) {
  stopMotors();
  afterPause = next;
  pauseLength = duration;
  phaseStarted = now;
  motion = Motion::Pause;
}

void beginTurn(int direction, Step next, uint32_t now) {
  stopMotors();
  turnDirection = direction;
  afterTurn = next;
  turnStartYaw = yaw;
  previousTurnError = 0;
  turnReachedTarget = false;
  phaseStarted = now;
  motion = Motion::Turn;
  Serial.printf("TURN: %s 90 degrees\n", direction < 0 ? "LEFT" : "RIGHT");
}

void beginDistanceMove(float targetMM, uint32_t now) {
  if (!encoderUsable(leftEncoder) || !encoderUsable(rightEncoder)) {
    stopRun("corner maneuver needs both encoders");
    return;
  }
  distanceLeftStart = leftEncoder.counts;
  distanceRightStart = rightEncoder.counts;
  distanceTarget = targetMM;
  phaseStarted = now;
  motion = Motion::DistanceMove;
  Serial.printf("%s %.0f mm\n", targetMM > 0 ? "CORNER_ADVANCE" : "CORNER_REVERSE", fabsf(targetMM));
}

bool frontBlocked() {
  return frontAPDS.raw > FRONT_BLOCKED;
}

int gyroCorrection() {
  return int(GYRO_Z_SIGN * (yaw - headingYaw) * GYRO_ASSIST);
}

void followWalls(uint32_t now) {
  const bool leftPresentForTurn = leftAPDS.filtered > SIDE_OPEN;
  const bool leftOpening = leftWallSeen && !leftPresentForTurn;
  leftWallSeen = leftPresentForTurn;
  if (leftOpening) {
    beginDistanceMove(CORNER_ADVANCE_MM, now);
    return;
  }
  if (frontBlocked()) {
    beginPause(Step::ChooseFrontTurn, 200, now);
    return;
  }

  const int leftValue = int(leftAPDS.filtered), rightValue = int(rightAPDS.filtered);
  const bool leftWall = leftValue > SIDE_PRESENT, rightWall = rightValue > SIDE_PRESENT;
  int error = 0;
  if (leftWall && rightWall) error = leftValue - rightValue;
  else if (leftWall) error = leftValue - WALL_TARGET;
  else if (rightWall) error = WALL_TARGET - rightValue;
  error = constrain(error, -MAX_WALL_ERROR, MAX_WALL_ERROR);
  const int steering = constrain(int(KP * error) + int(KD * (error - previousError)),
                                 -MAX_STEERING, MAX_STEERING);
  previousError = error;
  const int gyro = gyroCorrection();
  moveMotor(BASE_PWM + steering + gyro, BASE_PWM - steering - gyro);
}

void updateDistanceMove(uint32_t now) {
  if (!encoderUsable(leftEncoder) || !encoderUsable(rightEncoder)) {
    stopRun("encoder read or magnet fault during corner maneuver");
    return;
  }
  const float left = fabsf(float(leftEncoder.counts - distanceLeftStart));
  const float right = fabsf(float(rightEncoder.counts - distanceRightStart));
  const float traveledMM = (left + right) * 0.5f * PI * WHEEL_DIAMETER_MM / 4096.0f;
  const float remainingMM = fabsf(distanceTarget) - traveledMM;
  if (frontBlocked() || remainingMM <= 0) {
    beginPause(distanceTarget > 0 ? Step::LeftAtOpening : Step::Follow, 100, now);
  } else if (now - phaseStarted >= DISTANCE_TIMEOUT_MS) {
    stopRun("corner maneuver timed out");
  } else {
    const float remainingDegrees = remainingMM * 360.0f / (PI * WHEEL_DIAMETER_MM);
    const int speed = constrain(int(1.9f * remainingDegrees), 60, 125);
    const int drive = distanceTarget > 0 ? speed : -speed;
    const int gyro = gyroCorrection();
    moveMotor(drive + gyro, drive - gyro);
  }
}

void updateTurn(uint32_t now) {
  const float turned = fabsf(yaw - turnStartYaw);
  const int error = int(90.0f - turned);
  if (!turnReachedTarget && error <= 0) {
    turnReachedTarget = true;
    turnTargetAt = now;
  }
  if (now - phaseStarted >= TURN_TIMEOUT_MS ||
      (turnReachedTarget && now - turnTargetAt >= TURN_TARGET_HOLD_MS)) {
    headingYaw = yaw;
    if (!turnReachedTarget) {
      Serial.printf("TURN TIMEOUT: %.1f of 90 degrees. Rechecking walls.\n", turned);
    }
    beginPause(turnReachedTarget ? afterTurn : Step::Follow, 100, now);
    return;
  }
  int speed = constrain(int(TURN_KP * error + TURN_KD * (error - previousTurnError)),
                        -MAX_TURN_PWM, MAX_TURN_PWM);
  previousTurnError = error;
  if (speed > 0 && speed < MIN_TURN_PWM) speed = MIN_TURN_PWM;
  if (speed < 0 && speed > -MIN_TURN_PWM) speed = -MIN_TURN_PWM;
  moveMotor(turnDirection * speed, -turnDirection * speed);
}

void updatePause(uint32_t now) {
  if (now - phaseStarted < pauseLength) return;
  switch (afterPause) {
    case Step::Follow:
      previousError = 0;
      leftWallSeen = leftAPDS.filtered > SIDE_OPEN;
      motion = Motion::Follow;
      break;
    case Step::ChooseFrontTurn: {
      const bool leftWall = leftAPDS.filtered > SIDE_OPEN;
      const bool rightWall = rightAPDS.filtered > SIDE_OPEN;
      if (leftWall && rightWall) beginTurn(-1, Step::SecondLeft, now);
      else beginTurn(leftWall ? 1 : -1, Step::BackUp, now);
      break;
    }
    case Step::LeftAtOpening: beginTurn(-1, Step::BackUp, now); break;
    case Step::BackUp: beginDistanceMove(-TURN_REVERSE_MM, now); break;
    case Step::SecondLeft: beginTurn(-1, Step::DeadEndReverse, now); break;
    case Step::DeadEndReverse:
      phaseStarted = now;
      motion = Motion::DeadEndReverse;
      moveMotor(-BASE_PWM, -BASE_PWM);
      break;
  }
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

  if (motion == Motion::Idle) {
    if (samplesOK && !startRequested) updateGesture();
    if (startRequested) {
      startRequested = false;
      if (samplesOK) beginRun();
      else Serial.println("START BLOCKED: check the APDS readings and IMU.");
    }
    return;
  }

  if (!samplesOK || millis() - now > MAX_CONTROL_GAP_MS) {
    stopRun("navigation sensor unavailable or late");
    return;
  }

  const uint32_t decisionTime = millis();
  switch (motion) {
    case Motion::Follow: followWalls(decisionTime); break;
    case Motion::DistanceMove: updateDistanceMove(decisionTime); break;
    case Motion::Turn: updateTurn(decisionTime); break;
    case Motion::Pause: updatePause(decisionTime); break;
    case Motion::DeadEndReverse:
      if (decisionTime - phaseStarted >= DEAD_END_REVERSE_MS) {
        headingYaw = yaw;
        beginPause(Step::Follow, 100, decisionTime);
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
