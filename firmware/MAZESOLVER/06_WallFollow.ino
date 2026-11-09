// STEP 6 - Choose the next move and repeat.
bool hadLeftWall = false;
int previousWallError = 0;

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
    mpu.resetAngleZ();
    if (pauseWithSensors(100)) mpu.resetAngleZ();
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
