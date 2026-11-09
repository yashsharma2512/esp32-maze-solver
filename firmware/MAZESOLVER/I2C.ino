// TCA9548A and I2C - Select a channel and read or write a device register.

bool selectMux(uint8_t mask) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(mask);
  if (Wire.endTransmission() != 0) return false;
  return Wire.requestFrom(TCA_ADDR, uint8_t(1)) == 1 && Wire.read() == mask;
}

bool selectChannel(uint8_t channel) {
  return selectMux(uint8_t(1U << channel));
}

bool readReg(uint8_t address, uint8_t reg, uint8_t *out, uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, length) != length) return false;
  for (uint8_t i = 0; i < length; ++i) out[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
