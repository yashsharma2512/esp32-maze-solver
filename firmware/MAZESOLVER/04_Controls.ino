// STEP 4 - Handle start and stop commands.
enum class HandStep { Clear, Left, Right, Release };
HandStep handStep = HandStep::Clear;
bool running = false, startRequested = false;
const char *phase = "IDLE";
const char *lastStop = "none";

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
  if (!colorCalibrationActive && pressed && !wasPressed && millis() - lastPress >= 250) {
    lastPress = millis();
    if (running) stopRun("BOOT button");
    else startRequested = true;
  }
  wasPressed = pressed;

  if (colorCalibrationActive) {
    readColorCommands();
    return;
  }

  while (Serial.available()) {
    const int key = Serial.read();
    if (key == 'x' || key == 'X') stopRun("serial command");
    if ((key == 's' || key == 'S') && !running) startRequested = true;
    if ((key == 'c' || key == 'C') && !running) {
      startRequested = false;
      handStep = HandStep::Clear;
      beginColorCalibration();
      return;
    }
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

bool tryStartRun(bool sensorsOK) {
  if (sensorsOK && !startRequested) checkHandStart();
  if (startRequested) {
    startRequested = false;
    if (sensorsOK) {
      running = true;
      phase = "FOLLOW";
      mpu.resetAngleZ();
      hadLeftWall = leftAPDS.filtered > SIDE_OPEN;
      previousWallError = 0;
      lastStop = "none";
      Serial.println("START: left-wall following. Press BOOT or send x to stop.");
      return true;
    }
    Serial.println("START BLOCKED: check the APDS readings and IMU.");
  }
  return false;
}
