// STEP 3 - Start the sensors and read one set of navigation measurements.
bool apdsReady = false, gyroReady = false;
uint32_t lastSample = 0;

void setupSensors() {
  Serial.println("STEP 3: Checking the left, right, and front APDS, gyro, and color sensor.");

  apdsReady = true;
  for (const Proximity *sensor : {&leftAPDS, &rightAPDS, &frontAPDS}) {
    const bool ok = initAPDS(*sensor);
    apdsReady = apdsReady && ok;
    Serial.printf("APDS %s CH%u: %s\n", sensor->name, unsigned(sensor->channel),
                  ok ? "READY" : "INIT ERROR");
  }
  gyroReady = initIMU();
  Serial.println(gyroReady ? "Gyro calibrated." : "IMU startup failed. See the IMU error above.");
  setupColorSensor();
}

void printStatus() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 250) return;
  lastPrint = millis();
  Serial.printf("%s APDS L=%u R=%u F=%u | yaw=%+.1f | enc L=%ld(%s) R=%ld(%s) | PWM %d/%d | last_stop=%s\n",
                phase, unsigned(leftAPDS.raw), unsigned(rightAPDS.raw), unsigned(frontAPDS.raw),
                mpu.getAngleZ(), long(leftEncoder.counts), encoderUsable(leftEncoder) ? "OK" : "CHECK",
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
