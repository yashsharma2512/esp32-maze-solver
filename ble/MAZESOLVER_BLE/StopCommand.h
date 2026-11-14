#pragma once

#include <Arduino.h>

// Each input keeps its own match so split USB/BLE packets form one command.
enum class MotorCommand { None, Stop, Start, Reset };

class StopCommand {
  static constexpr char stopWord[] = "stop";
  static constexpr char startWord[] = "start";
  static constexpr char resetWord[] = "reset";
  bool matchesReset = true;
  bool matchesStop = true;
  bool matchesStart = true;
  size_t matched = 0;


public:
  void reset() {
    matched = 0;
    matchesStop = true;
    matchesStart = true;
    matchesReset = true;
  }

  MotorCommand read(uint8_t byte) {
    if (byte == '\r' || byte == '\n') {
      MotorCommand command = MotorCommand::None;
      if (matchesStop && matched == sizeof(stopWord) - 1) command = MotorCommand::Stop;
      if (matchesStart && matched == sizeof(startWord) - 1) command = MotorCommand::Start;
      if (matchesReset && matched == sizeof(resetWord) - 1) command = MotorCommand::Reset;
      reset();
      return command;
    }
    if (byte >= 'A' && byte <= 'Z') byte += 'a' - 'A';
    matchesStop = matchesStop && matched < sizeof(stopWord) - 1 && byte == stopWord[matched];
    matchesStart = matchesStart && matched < sizeof(startWord) - 1 && byte == startWord[matched];
    matchesReset = matchesReset && matched < sizeof(resetWord) - 1 && byte == resetWord[matched];
    if (matched < sizeof(startWord)) ++matched;
    return MotorCommand::None;
  }
};
