#include "MotorStop.h"
#include "StopCommand.h"
#include <freertos/queue.h>

// The Wi-Fi and USB tasks can stop the motors while the maze code is busy.
// Use the same lock for enabling and disabling STBY so a stop always wins.
static portMUX_TYPE motorStopLock = portMUX_INITIALIZER_UNLOCKED;
static bool stopLatched = false;
static bool resumePending = false;

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

bool requestMotorResume() {
  portENTER_CRITICAL(&motorStopLock);
  const bool paused = stopLatched;
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
  const bool firstStop = !stopLatched;
  stopLatched = true;
  resumePending = false;
  digitalWrite(STBY, LOW);
  portEXIT_CRITICAL(&motorStopLock);
  if (firstStop) Serial.println("STOP: motor drive paused. Send start to resume.");
}
static StaticQueue_t usbQueueStorage;
static uint8_t usbBytes[256];
static QueueHandle_t usbCommands = nullptr;
static StopCommand usbStopCommand;

static void receiveUsbCommands() {
  for (int byte = Serial.read(); byte >= 0; byte = Serial.read()) {
    const uint8_t value = byte;
    // g, b and x can arrive without a line ending in this sketch.
    const bool singleKey = usbStopCommand.atStart() &&
                           (value == 'g' || value == 'G' || value == 'b' ||
                            value == 'B' || value == 'x' || value == 'X');
    if (singleKey) {
      if (!motorStopRequested()) xQueueSend(usbCommands, &value, 0);
    } else {
      const MotorCommand command = usbStopCommand.read(value);
      if (command == MotorCommand::Stop) requestMotorStop();
      if (command == MotorCommand::Start) requestMotorResume();
    }
  }
}

void beginMotorStop() {
  usbCommands = xQueueCreateStatic(sizeof(usbBytes), sizeof(uint8_t), usbBytes, &usbQueueStorage);
  receiveUsbCommands();
  Serial.onReceive(receiveUsbCommands);
}

int readUsbCommand() {
  uint8_t byte;
  return xQueueReceive(usbCommands, &byte, 0) == pdTRUE ? byte : -1;
}
