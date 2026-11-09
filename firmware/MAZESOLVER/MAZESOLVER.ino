// ESP32 maze robot. Start with loop() in Step 6. See DEBUGGING.md for bench tests.
// Hardware.h contains the pins and sensor drivers.
#include "Hardware.h"

// STEP 1 - Settings. Pin and channel assignments are in Hardware.h.
// Wall-follow settings from the original robot.
constexpr int BASE_PWM = 100;
constexpr int FRONT_WALL = 40, SIDE_OPEN = 9, SIDE_WALL = 25;
constexpr int SIDE_TARGET = 50;
constexpr float WALL_KP = 0.85f, WALL_KD = 1.5f;
constexpr float GYRO_ASSIST = 1.5f;
constexpr int GYRO_Z_SIGN = 1;  // Set to -1 if the IMU's Z axis points down.

// Corner and turn settings from the original robot.
constexpr float WHEEL_DIAMETER_MM = 70.0f;
constexpr float CORNER_ADVANCE_MM = 220.0f, CORNER_BACKUP_MM = 30.0f;
constexpr bool TURN_LEFT = true, TURN_RIGHT = false;
constexpr float TURN_KP = 12.5f, TURN_KD = 0.2f;
constexpr uint32_t TURN_TIMEOUT_MS = 1100, TURN_HOLD_MS = 200;
constexpr uint32_t DISTANCE_TIMEOUT_MS = 4500;

constexpr int BOOT_BUTTON = 0;
constexpr uint32_t SAMPLE_MS = 10, SENSOR_LIMIT_MS = 120;
constexpr int HAND_NEAR = 150, HAND_AWAY = 100;

enum class HandStep { Clear, Left, Right, Release };
HandStep handStep = HandStep::Clear;
bool running = false, startRequested = false;
bool busReady = false, apdsReady = false, gyroReady = false;
bool hadLeftWall = false;
int previousWallError = 0;
float straightHeading = 0;
const char *phase = "IDLE";
const char *lastStop = "none";
uint32_t lastSample = 0;

// STEP 2 - Start the board and check which devices are ready.
void setup() {
  setupMotorPins();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  Serial.begin(115200);
  delay(750);
  Serial.println("\nMAZESOLVER | ESP32 Dev Module | left-wall following");
  Serial.println("STEP 2: Checking the I2C bus. Motors are in standby.");

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
  Serial.println("STEP 3: Checking the left, right, and front APDS, gyro, and color sensor.");

  apdsReady = true;
  for (const Proximity *sensor : {&leftAPDS, &rightAPDS, &frontAPDS}) {
    const bool ok = initAPDS(*sensor);
    apdsReady = apdsReady && ok;
    Serial.printf("APDS %s CH%u: %s\n", sensor->name, unsigned(sensor->channel),
                  ok ? "READY" : "INIT ERROR");
  }
  gyroReady = selectMux(0) && initIMU();
  Serial.println(gyroReady ? "Gyro calibrated." : "IMU init/calibration failed. Keep still and reset.");
  colorReady = initColor();
  Serial.println(colorReady ? "AS7341 CH2 ready. Color samples print while stopped."
                            : "AS7341 CH2 init failed. Check SD2/SC2.");
  Serial.println("STEP 4: Start with LEFT then RIGHT hand gestures, BOOT, or s. Stop with BOOT or x.");
}

// STEP 3 - Read sensors and report faults.
void printStatus() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 250) return;
  lastPrint = millis();
  Serial.printf("%s APDS L=%u R=%u F=%u | yaw=%+.1f | enc L=%ld(%s) R=%ld(%s) | PWM %d/%d | last_stop=%s\n",
                phase, unsigned(leftAPDS.raw), unsigned(rightAPDS.raw), unsigned(frontAPDS.raw),
                yaw, long(leftEncoder.counts), encoderUsable(leftEncoder) ? "OK" : "CHECK",
                long(rightEncoder.counts), encoderUsable(rightEncoder) ? "OK" : "CHECK",
                leftPWM, rightPWM, lastStop);
}

// Read one set of sensors. Timestamp each read so an intermittent fault is traceable.
bool readSensors() {
  const uint32_t start = millis();
  if (running && start - lastSample > SENSOR_LIMIT_MS) stopRun("navigation loop ran late");
  lastSample = start;
  if (!busReady) return false;

  const bool leftOK = apdsReady && sampleAPDS(leftAPDS);
  const uint32_t leftDone = millis();
  const bool rightOK = apdsReady && sampleAPDS(rightAPDS);
  const uint32_t rightDone = millis();
  const bool frontOK = apdsReady && sampleAPDS(frontAPDS);
  const uint32_t frontDone = millis();
  const bool gyroOK = gyroReady && sampleIMU();
  const uint32_t gyroDone = millis();
  const bool leftEncoderOK = sampleEncoder(leftEncoder);
  const uint32_t leftEncoderDone = millis();
  const bool rightEncoderOK = sampleEncoder(rightEncoder);
  const uint32_t end = millis();

  const bool sensorsOK = leftOK && rightOK && frontOK && gyroOK;
  if (running && (!sensorsOK || end - start > SENSOR_LIMIT_MS)) {
    const char *faultPhase = phase;
    stopRun("navigation sensor unavailable or late");
    Serial.printf("NAV FAULT phase=%s cycle=%ums limit=%ums | CH0=%s/%ums CH1=%s/%ums CH7=%s/%ums "
                  "IMU=%s/%ums ENC5=%s/%ums ENC4=%s/%ums\n",
                  faultPhase, unsigned(end - start), unsigned(SENSOR_LIMIT_MS),
                  leftOK ? "OK" : "FAIL", unsigned(leftDone - start),
                  rightOK ? "OK" : "FAIL", unsigned(rightDone - leftDone),
                  frontOK ? "OK" : "FAIL", unsigned(frontDone - rightDone),
                  gyroOK ? "OK" : "FAIL", unsigned(gyroDone - frontDone),
                  leftEncoderOK ? "OK" : "FAIL", unsigned(leftEncoderDone - gyroDone),
                  rightEncoderOK ? "OK" : "FAIL", unsigned(end - leftEncoderDone));
    Serial.println("STEP 3: Copy the NAV FAULT line and follow the sensor checks in DEBUGGING.md.");
  }
  printStatus();
  return sensorsOK;
}

void printColorSample() {
  const as7341_color_channel_t bands[] = {
      AS7341_CHANNEL_415nm_F1, AS7341_CHANNEL_445nm_F2, AS7341_CHANNEL_480nm_F3,
      AS7341_CHANNEL_515nm_F4, AS7341_CHANNEL_555nm_F5, AS7341_CHANNEL_590nm_F6,
      AS7341_CHANNEL_630nm_F7, AS7341_CHANNEL_680nm_F8,
      AS7341_CHANNEL_CLEAR, AS7341_CHANNEL_NIR,
  };
  Serial.print("AS7341 F1..F8,Clear,NIR:");
  for (auto band : bands) Serial.printf(" %u", unsigned(colorSensor.getChannel(band)));
  Serial.println();
}

// STEP 4 - Handle start and stop commands.
// Start or stop from USB Serial or the onboard BOOT button.
void stopRun(const char *reason) {
  stopMotors();
  running = false;
  startRequested = false;
  handStep = HandStep::Clear;
  phase = "IDLE";
  lastStop = reason;
  Serial.printf("STOP: %s\n", reason);
}

void readControls() {
  static bool wasPressed = digitalRead(BOOT_BUTTON) == LOW;
  static uint32_t lastPress = 0;
  const bool pressed = digitalRead(BOOT_BUTTON) == LOW;
  if (pressed && !wasPressed && millis() - lastPress >= 250) {
    lastPress = millis();
    if (running) stopRun("BOOT button");
    else startRequested = true;
  }
  wasPressed = pressed;

  while (Serial.available()) {
    const int key = Serial.read();
    if (key == 'x' || key == 'X') stopRun("serial command");
    if ((key == 's' || key == 'S') && !running) startRequested = true;
  }
}

void checkHandStart() {
  const bool leftClear = leftAPDS.raw < HAND_AWAY;
  const bool rightClear = rightAPDS.raw < HAND_AWAY;
  switch (handStep) {
    case HandStep::Clear:
      if (leftClear && rightClear) {
        handStep = HandStep::Left;
        Serial.println("Show a hand to the LEFT APDS (CH0), then move it away.");
      }
      break;
    case HandStep::Left:
      if (leftAPDS.raw > HAND_NEAR) {
        handStep = HandStep::Right;
        Serial.println("Show a hand to the RIGHT APDS (CH1), then move it away.");
      }
      break;
    case HandStep::Right:
      if (leftClear && rightAPDS.raw > HAND_NEAR) handStep = HandStep::Release;
      break;
    case HandStep::Release:
      if (leftClear && rightClear) {
        startRequested = true;
        handStep = HandStep::Clear;
      }
      break;
  }
}

// STEP 5 - Carry out a move while checking the sensors and stop button.
// Every movement loop calls this before its next motor command.
// BOOT, x, and sensor failures can stop any maneuver.
bool updateDuringMove() {
  while (running && millis() - lastSample < SAMPLE_MS) {
    readControls();
    delay(1);
  }
  readControls();
  if (!running) return false;
  readSensors();
  return running;
}

bool pauseWithSensors(uint32_t duration) {
  stopMotors();
  phase = "PAUSE";
  const uint32_t start = millis();
  while (millis() - start < duration) {
    if (!updateDuringMove()) return false;
  }
  return running;
}

bool frontIsBlocked() {
  return frontAPDS.raw > FRONT_WALL;
}

int headingCorrection() {
  return int(GYRO_Z_SIGN * (yaw - straightHeading) * GYRO_ASSIST);
}

void driveStraightBesideWalls() {
  const int left = int(leftAPDS.filtered), right = int(rightAPDS.filtered);
  int error = 0;
  if (left > SIDE_WALL && right > SIDE_WALL) error = left - right;
  else if (left > SIDE_WALL) error = left - SIDE_TARGET;
  else if (right > SIDE_WALL) error = SIDE_TARGET - right;

  error = constrain(error, -75, 75);
  const int steering = constrain(int(WALL_KP * error) +
                                int(WALL_KD * (error - previousWallError)), -120, 120);
  previousWallError = error;
  const int correction = steering + headingCorrection();
  moveMotor(BASE_PWM + correction, BASE_PWM - correction);
}

bool moveCornerDistance(float targetMM) {
  const bool forward = targetMM > 0;
  phase = forward ? "CORNER_ADVANCE" : "CORNER_REVERSE";
  if (!encoderUsable(leftEncoder) || !encoderUsable(rightEncoder)) {
    stopRun("corner maneuver needs both encoders");
    return false;
  }
  const int32_t leftStart = leftEncoder.counts, rightStart = rightEncoder.counts;
  const float wheelCircumference = PI * WHEEL_DIAMETER_MM;
  const uint32_t start = millis();
  Serial.printf("%s %.0f mm\n", phase, fabsf(targetMM));

  while (updateDuringMove()) {
    if (!encoderUsable(leftEncoder) || !encoderUsable(rightEncoder)) {
      stopRun("encoder read or magnet fault during corner maneuver");
      return false;
    }
    const float leftCounts = fabsf(float(leftEncoder.counts - leftStart));
    const float rightCounts = fabsf(float(rightEncoder.counts - rightStart));
    const float movedMM = (leftCounts + rightCounts) * 0.5f * wheelCircumference / 4096.0f;
    const float remainingMM = fabsf(targetMM) - movedMM;
    if (frontIsBlocked() || remainingMM <= 0) return pauseWithSensors(100);
    if (millis() - start >= DISTANCE_TIMEOUT_MS) {
      stopRun("corner maneuver timed out");
      return false;
    }
    const float remainingDegrees = remainingMM * 360.0f / wheelCircumference;
    const int speed = constrain(int(1.9f * remainingDegrees), 60, 125);
    const int drive = forward ? speed : -speed;
    const int gyro = headingCorrection();
    moveMotor(drive + gyro, drive - gyro);
  }
  return false;
}

// A timed-out pivot ends the maneuver. The next loop reads the walls again.
bool turn90(bool turnLeft) {
  stopMotors();
  phase = "TURN";
  const float startYaw = yaw;
  const uint32_t start = millis();
  int previousError = 0;
  bool reached = false;
  uint32_t reachedAt = 0;
  Serial.printf("TURN: %s 90 degrees\n", turnLeft ? "LEFT" : "RIGHT");

  while (updateDuringMove()) {
    const uint32_t now = millis();
    const float turned = fabsf(yaw - startYaw);
    const int error = int(90.0f - turned);
    if (!reached && error <= 0) {
      reached = true;
      reachedAt = now;
    }
    if (now - start >= TURN_TIMEOUT_MS || (reached && now - reachedAt >= TURN_HOLD_MS)) {
      stopMotors();
      straightHeading = yaw;
      if (!reached) Serial.printf("TURN TIMEOUT: %.1f of 90 degrees. Rechecking walls.\n", turned);
      return pauseWithSensors(100) && reached;
    }

    int speed = constrain(int(TURN_KP * error + TURN_KD * (error - previousError)), -165, 165);
    previousError = error;
    if (speed > 0 && speed < 55) speed = 55;
    if (speed < 0 && speed > -55) speed = -55;
    const int leftDrive = turnLeft ? -speed : speed;
    moveMotor(leftDrive, -leftDrive);
  }
  return false;
}

// STEP 6 - Choose the next move and repeat.
void handleFrontWall() {
  if (!pauseWithSensors(200)) return;
  const bool leftWall = leftAPDS.filtered > SIDE_OPEN;
  const bool rightWall = rightAPDS.filtered > SIDE_OPEN;

  if (leftWall && rightWall) {
    // Dead end: two left pivots followed by the original one-second backup.
    if (!turn90(TURN_LEFT) || !turn90(TURN_LEFT)) return;
    phase = "DEAD_END_REVERSE";
    moveMotor(-BASE_PWM, -BASE_PWM);
    const uint32_t start = millis();
    while (millis() - start < 1000) {
      if (!updateDuringMove()) return;
    }
    straightHeading = yaw;
    pauseWithSensors(100);
  } else if (turn90(leftWall ? TURN_RIGHT : TURN_LEFT)) {
    moveCornerDistance(-CORNER_BACKUP_MM);
  }
}

void followLeftWall() {
  phase = "FOLLOW";
  const bool leftWall = leftAPDS.filtered > SIDE_OPEN;
  const bool leftOpening = hadLeftWall && !leftWall;
  hadLeftWall = leftWall;

  if (leftOpening) {
    if (moveCornerDistance(CORNER_ADVANCE_MM) && turn90(TURN_LEFT)) {
      moveCornerDistance(-CORNER_BACKUP_MM);
    }
  } else if (frontIsBlocked()) {
    handleFrontWall();
  } else {
    driveStraightBesideWalls();
    return;
  }

  // Start a new steering calculation after a corner or turn attempt.
  previousWallError = 0;
  hadLeftWall = leftAPDS.filtered > SIDE_OPEN;
  if (running) phase = "FOLLOW";
}

void loop() {
  readControls();
  if (millis() - lastSample < SAMPLE_MS) {
    delay(1);
    return;
  }

  const bool sensorsOK = readSensors();
  if (running) {
    followLeftWall();
    return;
  }

  if (sensorsOK && !startRequested) checkHandStart();
  if (startRequested) {
    startRequested = false;
    if (sensorsOK) {
      running = true;
      phase = "FOLLOW";
      straightHeading = yaw;
      hadLeftWall = leftAPDS.filtered > SIDE_OPEN;
      previousWallError = 0;
      lastStop = "none";
      Serial.println("START: left-wall following. Press BOOT or send x to stop.");
      return;
    }
    Serial.println("START BLOCKED: check the APDS readings and IMU.");
  }

  if (colorReady && millis() - lastColorSample >= 600) {
    if (sampleColor()) printColorSample();
    else Serial.println("AS7341 CH2 read timed out or channel selection failed.");
    lastColorSample = millis();
  }
}
