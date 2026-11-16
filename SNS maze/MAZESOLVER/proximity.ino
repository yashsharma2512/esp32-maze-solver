#define SDA_PIN 16  //39
#define SCL_PIN 17  //40

void beginWire() {
  // Wire.setSCL(1);
  // Wire.setSDA(0);
  // Wire.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
}

bool TCA9548A(uint8_t bus) {
  Wire.beginTransmission(0x70);  // TCA9548A address is 0x70
  Wire.write(1 << bus);          // send byte to select bus
  return Wire.endTransmission() == 0;
}



void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t *readRegister(uint8_t reg, uint8_t len) {
  static uint8_t data[5];  // Assuming len is always <= 3
  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(APDS9960_ADDRESS, len);
  unsigned long _millis = millis();
  bool _err = false;
  while (Wire.available() < len) {
    if ((millis() - _millis) > 100) {
      Serial.println("error");
      _err = true;
      break;
    };
  }
  if (_err) {
    data[0] = 0;
    data[1] = 0;
    return data;
  }
  for (uint8_t i = 0; i < len; i++)
    data[i] = Wire.read();
  return data;
}

int readProximity(uint8_t channel) {
  TCA9548A(channel);
  uint8_t *proximityData = readRegister(APDS9960_REG_PDATA, 2);
  int proximity = (proximityData[1] << 8) | proximityData[0];
  return proximity;
}

void initAPDS(uint8_t channel) {
  TCA9548A(channel);
  delay(20);
  writeRegister(APDS9960_REG_ENABLE, (0 << 6) | (1 << 5) | (0 << 4) | (0 << 3) | (1 << 2) | (0 << 1) | 1);
  writeRegister(APDS9960_REG_CONFIG2, (0 << 7) | (0 << 6) | (0x03 << 4) | 1);
  // writeRegister(APDS9960_REG_CONFIG3, (0 << 5) | (0 << 3) | (0 << 2) | (0 << 1) | (0 << 0));
  writeRegister(APDS9960_REG_CONTROL, (0x01 << 6) | (0x02 << 2) | 0x01);
  // writeRegister(APDS9960_REG_COUNTR, (0x00 << 6));
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
  fr = readProximity(FR);
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
    pollColorCalibrationStart();
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
    digitalWrite(__arr[i], _logic[i]);
    delay(80);
    digitalWrite(__arr[i], !_logic[i]);
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
