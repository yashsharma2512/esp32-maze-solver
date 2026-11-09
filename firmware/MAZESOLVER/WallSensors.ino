// APDS-9960 - Read raw and smoothed proximity for the left, right, and front.
Proximity leftAPDS{"left", 0}, rightAPDS{"right", 1}, frontAPDS{"front", 7};

bool initAPDS(const Proximity &sensor) {
  uint8_t id = 0, enable = 0, pulse = 0;
  if (!selectChannel(sensor.channel) || !readReg(APDS_ADDR, 0x92, &id, 1) ||
      id != 0xAB) return false;
  // Match the working APDS test, including the eight IR pulses.
  if (!writeReg(APDS_ADDR, 0x80, 0x00) || !writeReg(APDS_ADDR, 0x90, 0x31) ||
      !writeReg(APDS_ADDR, 0x8F, 0x49) || !writeReg(APDS_ADDR, 0x8E, 0x87) ||
      !writeReg(APDS_ADDR, 0x80, 0x01)) return false;
  delay(10);
  return writeReg(APDS_ADDR, 0x80, 0x05) &&
         readReg(APDS_ADDR, 0x80, &enable, 1) && enable == 0x05 &&
         readReg(APDS_ADDR, 0x8E, &pulse, 1) && pulse == 0x87;
}

bool readAPDSProximity(Proximity &sensor) {
  // PDATA is the sensor's one-byte proximity reading (0 to 255).
  return selectChannel(sensor.channel) && readReg(APDS_ADDR, 0x9C, &sensor.raw, 1);
}

bool sampleAPDS(Proximity &sensor) {
  // Re-select the channel and retry once before treating a read as failed.
  if (!readAPDSProximity(sensor) && !readAPDSProximity(sensor)) {
    sensor.filterReady = false;
    return false;
  }
  sensor.filtered = sensor.filterReady ? APDS_ALPHA * sensor.filtered +
      (1.0f - APDS_ALPHA) * sensor.raw : sensor.raw;
  sensor.filterReady = true;
  return true;
}
