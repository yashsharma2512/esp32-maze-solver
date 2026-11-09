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
  return int(GYRO_Z_SIGN * mpu.getAngleZ() * GYRO_ASSIST);
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
  mpu.resetAngleZ();
  const uint32_t start = millis();
  int previousError = 0;
  bool reached = false;
  uint32_t reachedAt = 0;
  Serial.printf("TURN: %s 90 degrees\n", turnLeft ? "LEFT" : "RIGHT");

  while (updateDuringMove()) {
    const uint32_t now = millis();
    const float turned = fabsf(mpu.getAngleZ());
    const int error = int(90.0f - turned);
    if (!reached && error <= 0) {
      reached = true;
      reachedAt = now;
    }
    if (now - start >= TURN_TIMEOUT_MS || (reached && now - reachedAt >= TURN_HOLD_MS)) {
      stopMotors();
      mpu.resetAngleZ();
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
