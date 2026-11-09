#define SDA_PIN 21
#define SCL_PIN 22

void beginWire() {
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(20);
}

bool TCA9548A(uint8_t bus) {
  Wire.beginTransmission(0x70);
  Wire.write(1 << bus);
  return Wire.endTransmission() == 0;
}

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int readProximity(uint8_t channel) {
  if (TCA9548A(channel)) {
    Wire.beginTransmission(APDS9960_ADDRESS);
    Wire.write(APDS9960_REG_PDATA);
    // PDATA contains one proximity byte.
    if (Wire.endTransmission(false) == 0 &&
        Wire.requestFrom(uint8_t(APDS9960_ADDRESS), uint8_t(1)) == 1) {
      return Wire.read();
    }
  }
  Serial.println("error");
  return 0;
}

void initAPDS(uint8_t channel) {
  if (!TCA9548A(channel)) {
    Serial.println("error");
    return;
  }
  delay(20);
  writeRegister(APDS9960_REG_ENABLE, 0x00);
  writeRegister(APDS9960_REG_CONFIG2, 0x31);
  writeRegister(APDS9960_REG_CONTROL, 0x49);
  writeRegister(APDS9960_REG_COUNTR, 0x87);
  writeRegister(APDS9960_REG_ENABLE, 0x01);
  delay(10);
  writeRegister(APDS9960_REG_ENABLE, 0x05);
  delay(50);
}

int detectTurn(bool currDL, bool currDR) {
  if (currDL != prevWallLeft) {
    prevWallLeft = currDL;
    if (!currDL)
      return LEFT;
  } else if (currDR != prevWallRight) {
    prevWallRight = currDR;
    if (!currDR)
      return RIGHT;
  }
  return 0;
}

void updateWalls(bool _delay) {
  for (int i = 0; i < 5; i++) {
    updateProximitySensors();
    if (_delay)
      delay(5);
    else
      delay(2);
  }
  prevWallLeft = (dl_filtered > leftTurnSense);
  prevWallRight = (dr_filtered > rightTurnSense);
}

void updateProximitySensors() {
  fl = readProximity(FL);
  fr = fl;  // Use the same sample for both original front inputs.
  dl = readProximity(DL);
  dr = readProximity(DR);

  dl_filtered = alpha * dl_filtered + (1 - alpha) * dl;
  dr_filtered = alpha * dr_filtered + (1 - alpha) * dr;
  fl_filtered = alpha * fl_filtered + (1 - alpha) * fl;
  fr_filtered = alpha * fr_filtered + (1 - alpha) * fr;
}

void delayWithSensorUpdate(int ms) {  // only in multiples of 5
  long _curr = millis();
  delay(1);
  while ((millis() - _curr) < ms) {
    mpu.update();
    updateProximitySensors();
    readSerial();
    interpretSerial();
  }
}

void waitForUserInput1() {
  while (1) {
    updateProximitySensors();
    mpu.update();
    if (dl > 150)
      break;
    alterLEDstate();
    delay(50);
  }
  // _beep();
  state = HIGH;
  alterLEDstate();
}

void waitForUserInput2() {
  int __arr[] = { LED13, LEFTLED, RIGHTLED, LEFTLED };
  int _logic[] = { HIGH, LOW, LOW, LOW };
  for (int i = 0; i < 4; i++) {
    pinMode(__arr[i], OUTPUT);  // Set LED pins as outputs
  }
  int i = 0;
  while (1) {
    updateProximitySensors();
    mpu.update();
    if (dr > 150)
      break;
    if (__arr[i] >= 0) digitalWrite(__arr[i], _logic[i]);
    delay(80);
    if (__arr[i] >= 0) digitalWrite(__arr[i], !_logic[i]);
    i = (i + 1) % 4;
  }
  while (dr > 200) {
    updateProximitySensors();
    mpu.update();
  }
  _beep();
}

void printWalldata() {
  // SerialDebug.print(fl);
  // SerialDebug.print(",");
  // SerialDebug.print(fr);
  // SerialDebug.print(",");
  SerialDebug.print(dl);
  SerialDebug.print(",");
  SerialDebug.println(dr);
}
