// Start here. Follow the numbered tabs or open DEBUGGING.md to test one part.
#include "Robot.h"

void setup() {
  setupMotorPins();
  if (!startBoard()) return;  // Step 2
  setupSensors();             // Step 3
  Serial.println("STEP 4: Start with LEFT then RIGHT hand gestures, BOOT, or s. Stop with BOOT or x.");
  Serial.println("Color calibration: send c while stopped.");
}

void loop() {
  readControls();
  if (colorCalibrationActive || millis() - lastSample < SAMPLE_MS) {
    delay(1);
    return;
  }

  const bool sensorsOK = readSensors();
  if (running) {
    followLeftWall();
    return;
  }

  if (tryStartRun(sensorsOK)) return;
  reportColorWhileStopped();
}
