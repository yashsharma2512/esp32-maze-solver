// AS5600 - Track how far each wheel has turned.
Encoder leftEncoder{5}, rightEncoder{4};

bool sampleEncoder(Encoder &encoder) {
  uint8_t data[3]; // STATUS, RAW_ANGLE_H, RAW_ANGLE_L
  encoder.readOK = selectChannel(encoder.channel) &&
                   readReg(AS5600_ADDR, 0x0B, data, sizeof(data));
  if (!encoder.readOK) {
    encoder.previous = -1;
    return false;
  }
  encoder.status = data[0];
  int16_t angle = ((data[1] & 0x0F) << 8) | data[2];
  if (encoder.previous >= 0) {
    int delta = angle - encoder.previous;
    // Follow the wheel across the 4095-to-0 boundary.
    if (delta > 2048) delta -= 4096;
    if (delta < -2048) delta += 4096;
    encoder.counts += delta;
  }
  encoder.previous = angle;
  return true;
}

bool encoderUsable(const Encoder &encoder) {
  // Require a successful read and a detected magnet within range.
  return encoder.readOK && (encoder.status & 0x38) == 0x20;
}
