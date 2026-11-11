#include "BleSerialConsole.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
constexpr char BLE_NAME[] = "MazeSolver-BLE";
constexpr char UART_SERVICE[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char UART_RX[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char UART_TX[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

QueueHandle_t incoming = nullptr;
QueueHandle_t outgoing = nullptr;
BLECharacteristic *uartTx = nullptr;
std::atomic<uint32_t> activeSession{0};

class ConnectionCallbacks : public BLEServerCallbacks {
  uint32_t nextSession = 0;

  void onConnect(BLEServer *) override {
    xQueueReset(incoming);
    xQueueReset(outgoing);
    activeSession.store(++nextSession);
  }

  void onDisconnect(BLEServer *) override {
    activeSession.store(0);
    xQueueReset(incoming);
    xQueueReset(outgoing);
  }
};

class ReceiveCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const size_t length = characteristic->getLength();
    if (activeSession.load() == 0 || length > uxQueueSpacesAvailable(incoming)) return;
    const uint8_t *data = characteristic->getData();
    for (size_t i = 0; i < length; ++i) xQueueSend(incoming, &data[i], 0);
  }
};

void sendBleOutput(void *) {
  for (;;) {
    const uint32_t session = activeSession.load();
    if (session != 0) {
      uint8_t packet[20];  // Payload that fits the default BLE ATT MTU.
      size_t length = 0;
      while (length < sizeof(packet) && xQueueReceive(outgoing, &packet[length], 0) == pdTRUE) {
        ++length;
      }
      if (length != 0 && activeSession.load() == session) {
        uartTx->setValue(packet, length);
        uartTx->notify();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}  // namespace

BleSerialConsole SerialDebug;

void BleSerialConsole::begin() {
  incoming = xQueueCreate(512, sizeof(uint8_t));
  outgoing = xQueueCreate(4096, sizeof(uint8_t));
  if (!incoming || !outgoing) {
    Serial.println("BLE serial buffers unavailable.");
    return;
  }

  BLEDevice::init(BLE_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ConnectionCallbacks());
  server->advertiseOnDisconnect(true);

  BLEService *service = server->createService(UART_SERVICE);
  uartTx = service->createCharacteristic(UART_TX, BLECharacteristic::PROPERTY_NOTIFY);
  uartTx->addDescriptor(new BLE2902());
  BLECharacteristic *uartRx = service->createCharacteristic(
      UART_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  uartRx->setCallbacks(new ReceiveCallbacks());
  service->start();

  if (xTaskCreatePinnedToCore(sendBleOutput, "BLE serial", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("BLE serial task unavailable.");
    return;
  }
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(UART_SERVICE);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.println("BLE serial ready. Connect to MazeSolver-BLE.");
}

uint32_t BleSerialConsole::session() const {
  return activeSession.load();
}

int BleSerialConsole::readBle() {
  uint8_t byte;
  if (session() == 0 || xQueueReceive(incoming, &byte, 0) != pdTRUE) return -1;
  return byte;
}

size_t BleSerialConsole::write(uint8_t byte) {
  return write(&byte, 1);
}

size_t BleSerialConsole::write(const uint8_t *data, size_t length) {
  const size_t written = Serial.write(data, length);
  const uint32_t currentSession = session();
  for (size_t i = 0; i < length && currentSession != 0 && session() == currentSession; ++i) {
    if (xQueueSend(outgoing, &data[i], 0) != pdTRUE) break;
  }
  return written;
}
