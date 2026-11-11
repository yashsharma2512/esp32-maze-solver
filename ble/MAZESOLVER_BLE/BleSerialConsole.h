#pragma once

#include <Arduino.h>

class BleSerialConsole : public Print {
public:
  void begin();
  uint32_t session() const;
  int readBle();
  size_t write(uint8_t byte) override;
  size_t write(const uint8_t *data, size_t length) override;
};

extern BleSerialConsole SerialDebug;
