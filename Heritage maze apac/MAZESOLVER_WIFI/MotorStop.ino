#include "MotorStop.h"

// The Wi-Fi and USB tasks can stop the motors while the maze code is busy.
// Use the same lock for enabling and disabling STBY so a stop always wins.
static portMUX_TYPE motorStopLock = portMUX_INITIALIZER_UNLOCKED;
static bool stopLatched = false;
static bool resumePending = false;
static bool resetPending = false;

bool motorStopRequested() {
  portENTER_CRITICAL(&motorStopLock);
  const bool stopped = stopLatched;
  portEXIT_CRITICAL(&motorStopLock);
  return stopped;
}

bool enableMotorDrive() {
  portENTER_CRITICAL(&motorStopLock);
  const bool enabled = !stopLatched;
  if (enabled) digitalWrite(STBY, HIGH);
  portEXIT_CRITICAL(&motorStopLock);
  return enabled;
}

bool motorResumeRequested() {
  portENTER_CRITICAL(&motorStopLock);
  const bool requested = resumePending;
  portEXIT_CRITICAL(&motorStopLock);
  return requested;
}

bool requestMotorResume() {
  portENTER_CRITICAL(&motorStopLock);
  const bool paused = stopLatched && !resetPending;
  if (paused) resumePending = true;
  portEXIT_CRITICAL(&motorStopLock);
  return paused;
}

// The control loop clears the pause only after the old movement has returned.
bool takeMotorResumeRequest() {
  portENTER_CRITICAL(&motorStopLock);
  const bool requested = resumePending;
  if (requested) {
    stopLatched = false;
    resumePending = false;
  }
  portEXIT_CRITICAL(&motorStopLock);
  return requested;
}

void requestMotorStop() {
  portENTER_CRITICAL(&motorStopLock);
  const bool firstStop = !stopLatched && !resetPending;
  stopLatched = true;
  resumePending = false;
  digitalWrite(STBY, LOW);
  portEXIT_CRITICAL(&motorStopLock);
  if (firstStop) SerialDebug.println("STOP: motor drive paused. Send start to resume.");
}

bool prepareRobotReset() {
  portENTER_CRITICAL(&motorStopLock);
  const bool accepted = !resetPending;
  resetPending = true;
  stopLatched = true;
  resumePending = false;
  digitalWrite(STBY, LOW);
  portEXIT_CRITICAL(&motorStopLock);
  return accepted;
}

void restartRobot() {
  SerialDebug.println("RESET: rebooting ESP32.");
  delay(200);  // Give USB and wireless consoles time to send their acknowledgement.
  ESP.restart();
}
