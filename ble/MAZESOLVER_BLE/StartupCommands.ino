// Startup commands use separate line buffers for USB and BLE.
class StartupCommandInput {
  char line[sizeof("start")];
  size_t length = 0;
  bool tooLong = false;
  uint32_t previousSession = 0;

public:
  bool startRequested(uint32_t session, bool allowColorCalibration) {
    if (session != previousSession) {
      length = 0;
      tooLong = false;
      previousSession = session;
    }

    while (session == 0 || SerialDebug.session() == session) {
      const int key = session == 0 ? Serial.read() : SerialDebug.readBle();
      if (key < 0) break;
      if (allowColorCalibration && length == 0 && !tooLong && (key == 'c' || key == 'C')) {
        runColorCalibration(session);
        return false;
      }
      if (key == '\r' || key == '\n') {
        line[length] = '\0';
        const bool start = !tooLong && strcmp(line, "start") == 0;
        length = 0;
        tooLong = false;
        if (start) return true;
      } else if (length + 1 < sizeof(line)) {
        line[length++] = char(key);
      } else {
        tooLong = true;
      }
    }
    return false;
  }
};

bool checkStartupInput(bool allowColorCalibration) {
  static StartupCommandInput usb;
  static StartupCommandInput ble;
  if (usb.startRequested(0, allowColorCalibration)) return true;
  const uint32_t session = SerialDebug.session();
  return session != 0 && ble.startRequested(session, allowColorCalibration);
}
