// Standalone ESP32 Dev Module test. Open Serial Monitor at 115200 baud.
// Uses only the Wire library supplied by the ESP32 Arduino board package.
#include <Wire.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr uint8_t TCA_ADDR = 0x70;
// -1: MPU6050/MPU6500 connected directly to GPIO21/22, alongside the TCA.
// 0..7: IMU connected to that TCA channel instead.
constexpr int MPU_CHANNEL = -1;

uint8_t mpuAddr = 0;
uint8_t mpuId = 0;
bool tcaOK = false;

bool ping(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool setMux(uint8_t mask) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(mask);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(TCA_ADDR, (uint8_t)1) != 1) return false;
  return Wire.read() == mask;
}

bool readRegs(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, count) != count) return false;
  for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void scanBus() {
  bool any = false;
  for (uint8_t addr = 1; addr < 127; ++addr) {
    if (ping(addr)) {
      Serial.printf(" 0x%02X", addr);
      any = true;
    }
  }
  if (!any) Serial.print(" none");
  Serial.println();
}

int16_t signed16(const uint8_t *p) {
  return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nESP32 TCA9548A + MPU6050/MPU6500 TEST v2");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("FAIL: Could not start I2C.");
    return;
  }
  Wire.setTimeOut(50);

  tcaOK = ping(TCA_ADDR);
  if (tcaOK) {
    tcaOK = setMux(0);
    Serial.println(tcaOK ? "TCA 0x70: responding; channels disabled."
                         : "FAIL: TCA control register write/read.");
  } else {
    Serial.println("TCA 0x70: NOT FOUND. Check power, SDA/SCL, A0/A1/A2 and RESET.");
  }

  // Only classify this as the main bus after successfully isolating channels.
  if (tcaOK || !ping(TCA_ADDR)) {
    Serial.print("Main bus:");
    scanBus();
  }
  if (tcaOK) {
    for (uint8_t ch = 0; ch < 8; ++ch) {
      Serial.printf("TCA channel %u:", ch);
      if (!setMux((uint8_t)(1U << ch))) {
        Serial.println(" FAIL: channel control write/read");
        tcaOK = false;
        break;
      }
      scanBus();
    }
    bool isolated = setMux(0);
    tcaOK = tcaOK && isolated;
    Serial.println(tcaOK ? "TCA control test: PASS"
                         : "TCA control test: FAIL");
    if (!isolated) return;
    Serial.println("Main-bus devices also appear in every channel scan.");
  } else if (ping(TCA_ADDR)) {
    // A responding but unisolated mux could expose conflicting devices.
    Serial.println("MPU test stopped: TCA channels could not be isolated.");
    return;
  }

  if (MPU_CHANNEL < -1 || MPU_CHANNEL > 7) {
    Serial.println("FAIL: MPU_CHANNEL must be -1 or 0..7.");
    return;
  }
  if (MPU_CHANNEL >= 0 &&
      (!tcaOK || !setMux((uint8_t)(1U << MPU_CHANNEL)))) {
    Serial.println("FAIL: Cannot select MPU channel.");
    return;
  }

  const uint8_t addresses[] = {0x68, 0x69};
  for (uint8_t addr : addresses) {
    if (!ping(addr)) continue;
    uint8_t id = 0;
    if (!readRegs(addr, 0x75, &id, 1)) continue;
    Serial.printf("Device 0x%02X WHO_AM_I = 0x%02X\n", addr, id);
    // Require repeated matching reads before identifying the sensor.
    uint8_t confirm1 = 0, confirm2 = 0;
    if (!readRegs(addr, 0x75, &confirm1, 1) ||
        !readRegs(addr, 0x75, &confirm2, 1) ||
        confirm1 != id || confirm2 != id) {
      Serial.println("FAIL: Unstable identity reads. Check wiring/power.");
      continue;
    }
    if (id == 0x68 || id == 0x70) {
      mpuAddr = addr;
      mpuId = id;
      Serial.println(id == 0x70 ? "Identity matches MPU6500."
                               : "Identity matches MPU6050.");
      break;
    }
  }
  if (!mpuAddr) {
    Serial.println("No supported MPU6050/MPU6500 identity detected.");
    Serial.println("Check wiring and MPU_CHANNEL. Press EN after correcting.");
    return;
  }
  // Wake sensor, enable axes, select +/-2g and +/-250 degrees/second.
  bool configured = writeReg(0x6B, 0x01);
  delay(100);
  configured = writeReg(0x6C, 0x00) && configured;
  configured = writeReg(0x1B, 0x00) && configured;
  configured = writeReg(0x1C, 0x00) && configured;
  configured = writeReg(0x1A, 0x03) && configured;
  // MPU6500 has a separate accelerometer filter configuration register.
  if (mpuId == 0x70) configured = writeReg(0x1D, 0x03) && configured;
  if (!configured) {
    Serial.println("FAIL: MPU configuration write.");
    mpuAddr = 0;
    return;
  }
  Serial.println("IMU identity/config writes: PASS. Accel in g; gyro in degrees/second.");
  Serial.println("Move/rotate the module to check that readings change.");
}

void loop() {
  if (!mpuAddr) {
    delay(1000);
    return;
  }
  uint8_t data[14];
  if (!readRegs(mpuAddr, 0x3B, data, sizeof(data))) {
    Serial.println("FAIL: MPU read. Check wiring/power.");
    delay(500);
    return;
  }
  Serial.printf("Accel g: X=%+.3f Y=%+.3f Z=%+.3f | Gyro dps: X=%+.2f Y=%+.2f Z=%+.2f\n",
                signed16(data) / 16384.0f,
                signed16(data + 2) / 16384.0f,
                signed16(data + 4) / 16384.0f,
                signed16(data + 8) / 131.0f,
                signed16(data + 10) / 131.0f,
                signed16(data + 12) / 131.0f);
  delay(500);
}
