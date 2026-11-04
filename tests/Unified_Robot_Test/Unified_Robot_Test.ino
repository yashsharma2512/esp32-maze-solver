// ESP32-WROOM-32 unified maze robot hardware test.
// Uses the pin map in esp32-maze-solver/README.md. Motors never start by themselves.
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

constexpr uint8_t SDA_PIN = 21, SCL_PIN = 22;
constexpr uint8_t TCA_ADDR = 0x70, APDS_ADDR = 0x39, AS5600_ADDR = 0x36;
constexpr uint8_t LEFT_ENC_CH = 5, RIGHT_ENC_CH = 4;
constexpr int LMF = 25, LMR = 26, PWML = 27;
constexpr int RMF = 18, RMR = 19, PWMR = 23, STBY = 33;
constexpr int MOTOR_PWM = 120;
constexpr uint32_t MOTOR_MS = 350;
constexpr float APDS_ALPHA = 0.65f;

// Local access point. Change the password before use if desired (8+ characters).
const char *AP_SSID = "MazeRobot-Test";
const char *AP_PASSWORD = "maze-test-123";
WebServer web(80);
bool wifiReady = false, tcaReady = false, imuReady = false;
uint8_t imuAddr = 0, imuId = 0;
bool apdsFound[8] = {};
float apdsFiltered[8] = {};
bool apdsHasSample[8] = {};
uint8_t apdsRaw[8] = {}, apdsStatus[8] = {};
bool apdsReadOK[8] = {};

struct Encoder {
  const char *name;
  uint8_t channel;
  bool found;
  bool readOK;
  uint16_t raw;
  uint16_t zeroRaw;
  int16_t previousRaw;
  int32_t totalCounts;
  uint8_t status;
  uint8_t agc;
  uint16_t magnitude;
};
Encoder enc[2] = {
    {"LEFT", LEFT_ENC_CH, false, false, 0, 0, -1, 0, 0, 0, 0},
    {"RIGHT", RIGHT_ENC_CH, false, false, 0, 0, -1, 0, 0, 0, 0}};
float accel[3] = {}, gyro[3] = {};
bool imuReadOK = false;
String latestReport;
uint32_t lastSampleMs = 0, lastReportMs = 0;

bool ping(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool selectMux(uint8_t mask) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(mask);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(TCA_ADDR, uint8_t(1)) != 1) return false;
  return Wire.read() == mask;
}

bool readReg(uint8_t addr, uint8_t reg, uint8_t *out, uint8_t count) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, count) != count) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) out[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// APDS PGSAT is sticky until PICLEAR (0xE5) or PEN is disabled.
bool clearApdsSaturation() {
  Wire.beginTransmission(APDS_ADDR);
  Wire.write(uint8_t(0xE5));
  return Wire.endTransmission() == 0;
}

int16_t signed16(const uint8_t *p) {
  return int16_t((uint16_t(p[0]) << 8) | p[1]);
}

void stopMotors() {
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(LMF, LOW);
  digitalWrite(LMR, LOW);
  digitalWrite(RMF, LOW);
  digitalWrite(RMR, LOW);
  digitalWrite(STBY, LOW);
}

void initMotorPins() {
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);
  for (int pin : {LMF, LMR, PWML, RMF, RMR, PWMR}) pinMode(pin, OUTPUT);
  stopMotors();
}

void setDrive(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);
  left = int(left * 0.985f); // Same left trim as the existing motor test.
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);
  digitalWrite(LMF, left > 0 ? HIGH : LOW);
  digitalWrite(LMR, left < 0 ? HIGH : LOW);
  digitalWrite(RMF, right > 0 ? HIGH : LOW);
  digitalWrite(RMR, right < 0 ? HIGH : LOW);
  digitalWrite(STBY, HIGH);
  analogWrite(PWML, abs(left));
  analogWrite(PWMR, abs(right));
}

bool readEncoder(Encoder &e) {
  if (!e.found || !selectMux(uint8_t(1U << e.channel))) return false;
  uint8_t angle[2], mag[2];
  if (!readReg(AS5600_ADDR, 0x0C, angle, 2) ||
      !readReg(AS5600_ADDR, 0x0B, &e.status, 1) ||
      !readReg(AS5600_ADDR, 0x1A, &e.agc, 1) ||
      !readReg(AS5600_ADDR, 0x1B, mag, 2)) return false;
  e.raw = ((uint16_t(angle[0]) & 0x0F) << 8) | angle[1];
  e.magnitude = ((uint16_t(mag[0]) & 0x0F) << 8) | mag[1];
  if (e.previousRaw >= 0) {
    int16_t delta = int16_t(e.raw) - e.previousRaw;
    if (delta > 2048) delta -= 4096;
    if (delta < -2048) delta += 4096;
    e.totalCounts += delta;
  }
  e.previousRaw = e.raw;
  return true;
}

void tareEncoders() {
  if (!tcaReady) return;
  for (Encoder &e : enc) {
    if (!e.found) continue;
    e.previousRaw = -1;
    e.readOK = readEncoder(e);
    if (e.readOK) {
      e.zeroRaw = e.raw;
      e.totalCounts = 0;
      Serial.printf("%s CH%u tared\n", e.name, e.channel);
    }
  }
  selectMux(0);
}

const char *magnetLabel(uint8_t status) {
  if (!(status & 0x20)) return "NOT_DETECTED";
  if (status & 0x10) return "TOO_WEAK";
  if (status & 0x08) return "TOO_STRONG";
  return "OK";
}

bool initAPDS(uint8_t ch) {
  if (!selectMux(uint8_t(1U << ch))) return false;
  uint8_t id = 0;
  if (!readReg(APDS_ADDR, 0x92, &id, 1) || id != 0xAB) return false;
  // Values from the existing APDS test: eight 8-us pulses, original gain/boost.
  bool ok = writeReg(APDS_ADDR, 0x80, 0x00);
  ok = writeReg(APDS_ADDR, 0x90, 0x31) && ok;
  ok = writeReg(APDS_ADDR, 0x8F, 0x49) && ok;
  ok = writeReg(APDS_ADDR, 0x8E, 0x87) && ok;
  ok = writeReg(APDS_ADDR, 0x80, 0x01) && ok;
  delay(10);
  ok = writeReg(APDS_ADDR, 0x80, 0x05) && ok;
  delay(50);
  uint8_t enable = 0, pulse = 0;
  return ok && readReg(APDS_ADDR, 0x80, &enable, 1) &&
         readReg(APDS_ADDR, 0x8E, &pulse, 1) &&
         enable == 0x05 && pulse == 0x87;
}

bool initIMU() {
  if (tcaReady && !selectMux(0)) return false; // Isolate downstream devices.
  for (uint8_t addr : {uint8_t(0x68), uint8_t(0x69)}) {
    uint8_t id = 0;
    if (!ping(addr) || !readReg(addr, 0x75, &id, 1)) continue;
    if (id != 0x68 && id != 0x70) continue;
    imuAddr = addr;
    imuId = id;
    bool ok = writeReg(addr, 0x6B, 0x01); // Wake, clock from gyro.
    delay(100);
    ok = writeReg(addr, 0x6C, 0x00) && ok;
    ok = writeReg(addr, 0x1B, 0x00) && ok; // +/-250 dps
    ok = writeReg(addr, 0x1C, 0x00) && ok; // +/-2g
    ok = writeReg(addr, 0x1A, 0x03) && ok;
    if (id == 0x70) ok = writeReg(addr, 0x1D, 0x03) && ok;
    return ok;
  }
  return false;
}

void discoverDevices() {
  tcaReady = ping(TCA_ADDR) && selectMux(0);
  Serial.printf("TCA 0x70: %s\n", tcaReady ? "OK" : "NOT FOUND / CONTROL ERROR");
  if (tcaReady) {
    // Devices on the main bus would appear on every selected mux channel.
    bool mainApds = ping(APDS_ADDR);
    bool mainEncoder = ping(AS5600_ADDR);
    if (mainApds) Serial.println("WARNING: APDS 0x39 is on main bus; channel mapping ambiguous.");
    if (mainEncoder) Serial.println("WARNING: AS5600 0x36 is on main bus; channel mapping ambiguous.");
    for (uint8_t ch = 0; ch < 8; ++ch) {
      if (!selectMux(uint8_t(1U << ch))) continue;
      uint8_t id = 0;
      if (!mainApds && ping(APDS_ADDR) && readReg(APDS_ADDR, 0x92, &id, 1) && id == 0xAB) {
        apdsFound[ch] = initAPDS(ch);
        Serial.printf("CH%u APDS-9960: %s\n", ch,
                      apdsFound[ch] ? "READY" : "INIT ERROR");
      }
      if (!mainEncoder && ping(AS5600_ADDR)) {
        Serial.printf("CH%u AS5600 detected%s\n", ch,
                      ch == LEFT_ENC_CH ? " LEFT" :
                      ch == RIGHT_ENC_CH ? " RIGHT" : " (unexpected channel)");
        for (Encoder &e : enc) if (e.channel == ch) e.found = true;
      }
    }
    selectMux(0);
    tareEncoders();
  }
  // A responding mux with broken channel control may expose conflicting devices.
  imuReady = (!ping(TCA_ADDR) || tcaReady) && initIMU();
  Serial.printf("Main-bus IMU: %s", imuReady ? "READY" : "NOT FOUND / INIT ERROR");
  if (imuReady) Serial.printf(" (0x%02X, WHO_AM_I=0x%02X)", imuAddr, imuId);
  Serial.println();
}

void sampleSensors() {
  if (tcaReady) {
    for (uint8_t ch = 0; ch < 8; ++ch) {
      if (!apdsFound[ch]) continue;
      uint8_t status = 0, raw = 0;
      apdsReadOK[ch] = selectMux(uint8_t(1U << ch)) &&
                       readReg(APDS_ADDR, 0x93, &status, 1) &&
                       readReg(APDS_ADDR, 0x9C, &raw, 1);
      if (apdsReadOK[ch]) {
        apdsStatus[ch] = status;
        apdsRaw[ch] = raw;
        if ((status & 0x40) && !clearApdsSaturation()) apdsReadOK[ch] = false;
        if (status & 0x02) {
          apdsFiltered[ch] = apdsHasSample[ch] ?
              APDS_ALPHA * apdsFiltered[ch] + (1.0f - APDS_ALPHA) * raw : raw;
          apdsHasSample[ch] = true;
        }
      }
    }
    for (Encoder &e : enc) if (e.found) e.readOK = readEncoder(e);
    selectMux(0);
  }
  if (imuReady) {
    uint8_t data[14];
    imuReadOK = readReg(imuAddr, 0x3B, data, sizeof(data));
    if (imuReadOK) {
      for (int i = 0; i < 3; ++i) {
        accel[i] = signed16(data + i * 2) / 16384.0f;
        gyro[i] = signed16(data + 8 + i * 2) / 131.0f;
      }
    }
  }
}

void buildReport() {
  String s;
  s.reserve(1200);
  char line[190];
  s += "ESP32 robot diagnostic | motors idle unless commanded\n";
  s += "I2C GPIO21/22 | TCA 0x70: ";
  s += tcaReady ? "OK\n" : "ERROR\n";
  if (imuReady && imuReadOK) {
    snprintf(line, sizeof(line),
             "IMU 0x%02X ID=0x%02X accel[g]=%+.2f,%+.2f,%+.2f gyro[dps]=%+.1f,%+.1f,%+.1f\n",
             imuAddr, imuId, accel[0], accel[1], accel[2],
             gyro[0], gyro[1], gyro[2]);
    s += line;
  } else s += imuReady ? "IMU READ ERROR\n" : "IMU NOT READY\n";
  for (uint8_t ch = 0; ch < 8; ++ch) {
    if (!apdsFound[ch]) continue;
    if (!apdsReadOK[ch]) {
      snprintf(line, sizeof(line), "APDS CH%u READ ERROR\n", ch);
    } else {
      snprintf(line, sizeof(line),
               "APDS CH%u raw=%3u filtered=%6.1f valid=%u saturated=%u\n",
               ch, apdsRaw[ch], apdsFiltered[ch],
               !!(apdsStatus[ch] & 0x02), !!(apdsStatus[ch] & 0x40));
    }
    s += line;
  }
  if (tcaReady) {
    bool any = false;
    for (bool found : apdsFound) any |= found;
    if (!any) s += "APDS: none detected\n";
  }
  for (const Encoder &e : enc) {
    if (!e.found) {
      snprintf(line, sizeof(line), "%s encoder CH%u NOT FOUND\n", e.name, e.channel);
    } else if (!e.readOK) {
      snprintf(line, sizeof(line), "%s encoder CH%u READ ERROR\n", e.name, e.channel);
    } else {
      float angle = ((e.raw + 4096 - e.zeroRaw) & 0x0FFF) * (360.0f / 4096.0f);
      float total = e.totalCounts * (360.0f / 4096.0f);
      snprintf(line, sizeof(line),
               "%s encoder CH%u raw=%4u angle=%7.2f total=%9.2f mag=%4u AGC=%3u %s\n",
               e.name, e.channel, e.raw, angle, total,
               e.magnitude, e.agc, magnetLabel(e.status));
    }
    s += line;
  }
  latestReport = s;
  Serial.println(s);
}

bool runMotorTest(const String &action) {
  int left = 0, right = 0;
  if (action == "lf") left = MOTOR_PWM;
  else if (action == "lb") left = -MOTOR_PWM;
  else if (action == "rf") right = MOTOR_PWM;
  else if (action == "rb") right = -MOTOR_PWM;
  else if (action == "bf") left = right = MOTOR_PWM;
  else if (action == "bb") left = right = -MOTOR_PWM;
  else return false;
  Serial.printf("MOTOR %s: %d ms at PWM %d\n",
                action.c_str(), MOTOR_MS, MOTOR_PWM);
  setDrive(left, right);
  delay(MOTOR_MS); // No I2C or web work while the motor driver is enabled.
  stopMotors();
  Serial.println("MOTOR STOP; STBY LOW");
  return true;
}

void setupWeb() {
  WiFi.mode(WIFI_AP);
  wifiReady = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (!wifiReady) {
    Serial.println("Wi-Fi AP failed; USB Serial remains available.");
    return;
  }
  web.on("/", HTTP_GET, []() {
    const char page[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Maze robot test</title><style>
body{font:16px system-ui;max-width:850px;margin:24px auto;padding:0 16px;background:#101820;color:#edf5fa}
pre{overflow:auto;background:#202e38;padding:16px;border-radius:8px;line-height:1.5}
button{padding:12px;margin:4px;border:0;border-radius:6px;background:#3ca1da;color:#07131a;font-weight:bold}
.warn{color:#ffcc66}</style></head><body>
<h1>ESP32 maze robot test</h1>
<p>Connect to MazeRobot-Test, then open 192.168.4.1. USB Serial also prints at 115200 baud.</p>
<pre id="status">Loading...</pre>
<form method="post" action="/tare"><button>Tare both encoders</button></form>
<h2>Motor tests</h2><p class="warn">Lift the wheels before pressing a motor button. Each pulse lasts 350 ms; STBY then goes LOW.</p>
<form method="post" action="/motor">
<button name="action" value="lf">Left forward</button><button name="action" value="lb">Left reverse</button>
<button name="action" value="rf">Right forward</button><button name="action" value="rb">Right reverse</button>
<button name="action" value="bf">Both forward</button><button name="action" value="bb">Both reverse</button>
</form>
<script>async function update(){try{document.getElementById('status').textContent=await(await fetch('/status',{cache:'no-store'})).text()}catch(e){}}update();setInterval(update,1000)</script>
</body></html>)HTML";
    web.send_P(200, "text/html", page);
  });
  web.on("/status", HTTP_GET, []() {
    web.send(200, "text/plain", latestReport);
  });
  web.on("/tare", HTTP_POST, []() {
    tareEncoders();
    web.sendHeader("Location", "/", true);
    web.send(303, "text/plain", "Tared");
  });
  web.on("/motor", HTTP_POST, []() {
    if (!runMotorTest(web.arg("action"))) {
      web.send(400, "text/plain", "Unknown motor command");
      return;
    }
    web.sendHeader("Location", "/", true);
    web.send(303, "text/plain", "Motor pulse complete");
  });
  web.begin();
  Serial.printf("Wi-Fi AP: %s | password: %s | open http://%s/\n",
                AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  initMotorPins(); // STBY LOW before any scanning or Wi-Fi startup.
  delay(500);
  Serial.println("\nESP32 unified hardware test: no automatic motor movement");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("I2C start failed");
  } else {
    Wire.setTimeOut(50);
    discoverDevices();
    sampleSensors();
  }
  buildReport();
  setupWeb();
  Serial.println("USB commands: z=tare, 1/2=left forward/reverse, 3/4=right forward/reverse, 5/6=both forward/reverse.");
}

void loop() {
  if (wifiReady) web.handleClient();
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'z' || c == 'Z') tareEncoders();
    else if (c == '1') runMotorTest("lf");
    else if (c == '2') runMotorTest("lb");
    else if (c == '3') runMotorTest("rf");
    else if (c == '4') runMotorTest("rb");
    else if (c == '5') runMotorTest("bf");
    else if (c == '6') runMotorTest("bb");
  }
  uint32_t now = millis();
  if (now - lastSampleMs >= 100) {
    lastSampleMs = now;
    sampleSensors();
  }
  if (now - lastReportMs >= 1000) {
    lastReportMs = now;
    buildReport();
  }
}
