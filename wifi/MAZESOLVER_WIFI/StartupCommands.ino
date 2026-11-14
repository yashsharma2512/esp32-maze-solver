// Startup commands use separate line buffers for USB and the browser console.
class StartupCommandInput {
  char line[sizeof("camera")];
  size_t length = 0;
  bool tooLong = false;

public:
  bool startRequested(bool fromNetwork, bool allowColorCalibration) {
    while (true) {
      const int key = fromNetwork ? SerialDebug.readNetwork() : SerialDebug.readUsb();
      if (key < 0) break;
      if (key == '\r' || key == '\n') {
        line[length] = '\0';
        const bool start = !tooLong && strcmp(line, "start") == 0;
        const bool calibrate = allowColorCalibration && !tooLong && length == 1 &&
                               (line[0] == 'c' || line[0] == 'C');
        if (!tooLong && strcmp(line, "camera") == 0) printCameraStatus();
        length = 0;
        tooLong = false;
        if (calibrate) {
          runColorCalibration(fromNetwork);
          return false;
        }
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
  if (motorStopRequested()) return false;
  readSerial();
  SerialDebug.handleOta();
  static StartupCommandInput usb;
  static StartupCommandInput web;
  return usb.startRequested(false, allowColorCalibration) ||
         web.startRequested(true, allowColorCalibration);
}
