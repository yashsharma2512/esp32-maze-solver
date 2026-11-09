// MPU - Track the heading with the original MPU6050_light library.
MPU6050 mpu(Wire);

bool sampleIMU() {
  // Retry the communication check once because mpu.update() returns no status.
  uint8_t data[2];
  if (!readReg(mpu.getAddress(), 0x47, data, sizeof(data)) &&
      !readReg(mpu.getAddress(), 0x47, data, sizeof(data))) return false;
  mpu.update();
  return true;
}

bool initIMU() {
  // 1. Find the IMU on the main bus and read its model ID.
  if (!selectMux(0)) {
    Serial.println("IMU ERROR: could not close the TCA channels before checking the main bus.");
    return false;
  }
  for (uint8_t address : {uint8_t(0x68), uint8_t(0x69)}) {
    uint8_t id = 0;
    if (!readReg(address, 0x75, &id, 1)) continue;
    if (id != 0x68 && id != 0x70) {
      Serial.printf("IMU at 0x%02X returned unsupported ID 0x%02X.\n", unsigned(address), unsigned(id));
      continue;
    }
    mpu.setAddress(address);
    Serial.printf("IMU %s at 0x%02X, ID=0x%02X.\n",
                  id == 0x68 ? "MPU6050" : "MPU6500", unsigned(address), unsigned(id));

    // 2. Initialize the library with the exact settings from original mpu.ino.
    const uint8_t status = mpu.begin(3, 0);
    if (status != 0) {
      Serial.printf("IMU ERROR: mpu.begin(3, 0) returned %u.\n", unsigned(status));
      return false;
    }
    uint8_t gyroConfig = 0;
    if (!readReg(address, 0x1B, &gyroConfig, 1) || (gyroConfig & 0x18) != 0x18) {
      Serial.println("IMU ERROR: gyro range setting did not read back.");
      return false;
    }
    uint8_t accelConfig = 0;
    if (!readReg(address, 0x1C, &accelConfig, 1) || (accelConfig & 0x18) != 0x00) {
      Serial.println("IMU ERROR: accelerometer range setting did not read back.");
      return false;
    }
    Serial.println("Gyro range +/-2000 deg/s. Accelerometer range +/-2 g. Keep still.");

    // 3. Calibrate the gyro and accelerometer as in the original sketch.
    mpu.calcOffsets(true, true);
    if (!sampleIMU()) {
      Serial.println("IMU ERROR: gyro read failed after calibration.");
      return false;
    }
    mpu.resetAngleZ();
    return true;
  }
  Serial.println("IMU ERROR: no MPU6050 or MPU6500 found at 0x68 or 0x69. Check the main I2C wiring.");
  return false;
}
