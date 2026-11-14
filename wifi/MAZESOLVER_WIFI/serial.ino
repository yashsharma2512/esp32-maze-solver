#include <errno.h>

// The Nicla sends one line per frame: type,stable_ms,x,y.
static constexpr unsigned long CAMERA_TIMEOUT_MS = 200;
static constexpr int CAMERA_FRAME_SIZE = 160;
static constexpr int NO_VICTIM = -1;

static char cameraLine[32];
static size_t cameraLineLength = 0;
static bool cameraLineTooLong = false;
static bool cameraPacketReceived = false;
static unsigned long lastCameraRead = 0;
static unsigned long lastCameraPacket = 0;
static int victimType = NO_VICTIM;
static unsigned long elapsedTime = 0;
static int victimX = -1;
static int victimY = -1;
static bool victimHandled = false;

static void interpretCameraLine(const char *line) {
  long values[4];
  for (size_t field = 0; field < 4; ++field) {
    char *end;
    errno = 0;
    values[field] = strtol(line, &end, 10);
    if (end == line || errno == ERANGE) return;
    if (*end != (field == 3 ? '\0' : ',')) return;
    line = end + 1;
  }

  const long type = values[0];
  const long age = values[1];
  const long x = values[2];
  const long y = values[3];
  if (type == NO_VICTIM) {
    if (age != 0 || x != -1 || y != -1) return;
    victimHandled = false;
  } else if (type < 0 || type > 2 || age < 0 ||
             x < 0 || x >= CAMERA_FRAME_SIZE || y < 0 || y >= CAMERA_FRAME_SIZE) {
    return;
  }

  victimType = type;
  elapsedTime = age;
  victimX = x;
  victimY = y;
  lastCameraPacket = millis();
  cameraPacketReceived = true;
}

void readSerial() {
  const unsigned long now = millis();
  if (now - lastCameraRead > CAMERA_TIMEOUT_MS) {
    // Discard queued data after a long turn or other gap in polling.
    while (Serial1.read() >= 0) {}
    cameraLineLength = 0;
    cameraLineTooLong = false;
  }
  lastCameraRead = now;

  for (int byte = Serial1.read(); byte >= 0; byte = Serial1.read()) {
    if (byte == '\r') continue;
    if (byte == '\n') {
      if (!cameraLineTooLong && cameraLineLength != 0) {
        cameraLine[cameraLineLength] = '\0';
        interpretCameraLine(cameraLine);
      }
      cameraLineLength = 0;
      cameraLineTooLong = false;
    } else if (byte < 32 || byte > 126 || cameraLineLength == sizeof(cameraLine) - 1) {
      cameraLineTooLong = true;
    } else if (!cameraLineTooLong) {
      cameraLine[cameraLineLength++] = char(byte);
    }
  }
}

static bool cameraLinkActive() {
  return cameraPacketReceived && millis() - lastCameraPacket <= CAMERA_TIMEOUT_MS;
}

static const char *cameraVictimName() {
  switch (victimType) {
    case 2: return "HARMED";
    case 1: return "UNHARMED";
    case 0: return "STABLE";
    default: return "no target";
  }
}

void printCameraStatus() {
  if (!cameraLinkActive()) {
    SerialDebug.println("Camera: no recent Nicla packet. Check power, UART and ground.");
    return;
  }
  if (victimType == NO_VICTIM) {
    SerialDebug.println("Camera: linked, no target.");
    return;
  }
  SerialDebug.printf("Camera: %s | x=%d y=%d | stable %lu ms\n",
                     cameraVictimName(), victimX, victimY, elapsedTime);
}

void victimDetection() {
  readSerial();
  if (!cameraLinkActive() || victimType == NO_VICTIM || victimHandled || elapsedTime <= 5) return;

  // Keep the original approach and victim indication.
  moveMotor(0, 0);
  while (!motorStopRequested()) {
    mpu.update();
    updateProximitySensors();
    readSerial();
    if (!cameraLinkActive() || victimType == NO_VICTIM) {
      moveMotor(0, 0);
      return;
    }

    if (victimX <= 70) {
      moveMotor(-45, -45);
      continue;
    }

    moveMotor(0, 0);
    if (elapsedTime <= 500) continue;

    SerialDebug.printf("Found victim: %s\n", cameraVictimName());
    SerialDebug.println("Waiting 5 sec...");
    uint32_t color = strip.Color(0, 100, 0);
    if (victimType == 2) color = strip.Color(100, 0, 0);
    if (victimType == 1) color = strip.Color(100, 50, 0);

    victimHandled = true;
    for (int flash = 0; flash < 5; ++flash) {
      showColor(color);
      delayWithSensorUpdate(500);
      strip.clear();
      strip.show();
      if (motorStopRequested()) return;
      delayWithSensorUpdate(500);
      if (motorStopRequested()) return;
    }

    moveMotor(100, 100);
    delayWithSensorUpdate(500);
    return;
  }
}
