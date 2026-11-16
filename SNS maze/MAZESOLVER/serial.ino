// Nicla Vision sends separate ring and PSO packets over UART1.
// The shared reader validates frames and requires three matching detections.
static void readCameraStatusCommand() {
  static char line[12];
  static uint8_t length = 0;
  static bool overflow = false;
  while (SerialDebug.available()) {
    const int key = SerialDebug.read();
    if (key == '\r' || key == '\n') {
      line[length] = '\0';
      if (!overflow && strcmp(line, "camera") == 0) {
        niclaVision.printStatus(SerialDebug);
      }
      length = 0;
      overflow = false;
    } else if (length + 1 < sizeof(line)) {
      line[length++] = char(key);
    } else {
      overflow = true;
    }
  }
}

void serviceCameraTargets() {
  if (!cameraArmed) return;
  niclaVision.service(Serial1, SerialDebug);
  readCameraStatusCommand();
  static uint32_t firstPollAt = millis();
  static bool initialStatusPrinted = false;
  if (!initialStatusPrinted && millis() - firstPollAt >= 1000) {
    niclaVision.printStatus(SerialDebug);
    initialStatusPrinted = true;
  }
  if (rescueSequence.active()) return;
  updateFloorFlash();
  if (floorFlashActive) return;
  targetPixels.showTargets(strip, niclaVision.confirmedRing(), niclaVision.confirmedLetter());
}

bool serviceRescueTarget() {
  if (!cameraArmed) return false;
  serviceCameraTargets();
  if (floorFlashActive) return false;
  return rescueSequence.handle(niclaVision, targetPixels, strip, SerialDebug);
}
