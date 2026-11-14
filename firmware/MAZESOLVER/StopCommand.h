#pragma once

#include <Arduino.h>

// Each input keeps its own match so split USB/BLE packets form one command.
class StopCommand {
  static constexpr char word[] = "stop";
  size_t matched = 0;
  bool otherCommand = false;

public:
  void reset() {
    matched = 0;
    otherCommand = false;
  }

  bool read(uint8_t byte) {
    if (byte == '\r' || byte == '\n') {
      const bool stop = !otherCommand && matched == sizeof(word) - 1;
      reset();
      return stop;
    }
    if (!otherCommand && matched < sizeof(word) - 1 &&
        (byte == word[matched] || byte == word[matched] - 'a' + 'A')) {
      ++matched;
    } else {
      otherCommand = true;
    }
    return false;
  }
};
