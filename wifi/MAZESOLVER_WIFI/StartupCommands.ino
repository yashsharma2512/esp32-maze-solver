// Startup commands use separate line buffers for USB and the browser console.
class StartupCommandInput {
  char line[sizeof("start")];
  size_t length = 0;
  bool tooLong = false;

public:
  bool startRequested(bool fromNetwork, bool allowColorCalibration) {
    while (true) {
      const int key = fromNetwork ? SerialDebug.readNetwork() : Serial.read();
      if (key < 0) break;
      if (allowColorCalibration && length == 0 && !tooLong && (key == 'c' || key == 'C')) {
        runColorCalibration(fromNetwork);
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
  SerialDebug.handleOta();
  static StartupCommandInput usb;
  static StartupCommandInput web;
  return usb.startRequested(false, allowColorCalibration) ||
         web.startRequested(true, allowColorCalibration);
}
