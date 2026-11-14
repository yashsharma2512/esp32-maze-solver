#include "MotorStop.h"

// The Wi-Fi and USB tasks can stop the motors while the maze code is busy.
// Use the same lock for enabling and disabling STBY so a stop always wins.
static portMUX_TYPE motorStopLock = portMUX_INITIALIZER_UNLOCKED;
static bool stopLatched = false;

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

void requestMotorStop() {
  portENTER_CRITICAL(&motorStopLock);
  const bool firstStop = !stopLatched;
  stopLatched = true;
  digitalWrite(STBY, LOW);
  portEXIT_CRITICAL(&motorStopLock);
  if (firstStop) SerialDebug.println("STOP: motor drive disabled. Reset the ESP32 to start a new run.");
}
