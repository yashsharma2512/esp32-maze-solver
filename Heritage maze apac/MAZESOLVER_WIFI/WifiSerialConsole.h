#pragma once

#include <Arduino.h>

class WifiSerialConsole : public Print {
public:
  void begin();
  void handleOta();
  void endStartup();
  int readUsb();
  int readNetwork();
  size_t write(uint8_t byte) override;
  size_t write(const uint8_t *data, size_t length) override;
};

extern WifiSerialConsole SerialDebug;
