#pragma once

#include <Arduino.h>
#include <errno.h>
#include <stdlib.h>

class NiclaVisionReader {
 public:
  void poll(Stream &input) {
    const unsigned long now = millis();
    int available = input.available();
    if (now - lastPoll_ > timeoutMs_) {
      // A turn can leave old detections queued. Wait for the next fresh frame.
      while (available-- > 0 && input.read() >= 0) ++receivedBytes_;
      length_ = 0;
      discardLine_ = false;
      lastPoll_ = now;
      return;
    }
    lastPoll_ = now;

    while (available-- > 0) {
      const int byte = input.read();
      if (byte < 0) break;
      ++receivedBytes_;
      if (byte == '\r') continue;
      if (byte == '\n') {
        if (!discardLine_ && length_ != 0) {
          line_[length_] = '\0';
          parseLine(now);
        }
        length_ = 0;
        discardLine_ = false;
      } else if (byte < 32 || byte > 126 || length_ == sizeof(line_) - 1) {
        discardLine_ = true;
      } else if (!discardLine_) {
        line_[length_++] = char(byte);
      }
    }
  }

  void service(Stream &input, Print &output) {
    poll(input);
    report(ring_, "COGNITIVE", output);
    report(letter_, "PSO", output);
  }

  int confirmedRing() const { return confirmedType(ring_); }
  int confirmedLetter() const { return confirmedType(letter_); }

  int rescueTarget() const {
    const int ring = confirmedRing();
    return ring >= 0 ? ring : confirmedLetter();
  }

  bool targetsCleared() const { return cleared(ring_) && cleared(letter_); }

  void printStatus(Print &output) const {
    output.printf("Camera UART: %lu bytes received, %lu packets accepted since boot.\n",
                  receivedBytes_, acceptedPackets_);
    if (!fresh(ring_) && !fresh(letter_)) {
      output.println("Camera: no recent Nicla packet. Check power, UART and ground.");
      return;
    }
    printTarget(ring_, "Camera ring", output);
    printTarget(letter_, "Camera PSO", output);
  }

 private:
  static constexpr unsigned long timeoutMs_ = 200;
  static constexpr uint8_t confirmFrames_ = 3;
  static constexpr int coordinateLimit_ = 160;

  struct Target {
    bool received = false;
    unsigned long lastPacket = 0;
    unsigned long age = 0;
    int type = -1;
    int x = -1;
    int y = -1;
    int reportedType = -1;
    uint8_t frames = 0;
  };

  Target ring_;
  Target letter_;
  unsigned long receivedBytes_ = 0;
  unsigned long acceptedPackets_ = 0;
  unsigned long lastPoll_ = 0;
  char line_[32];
  size_t length_ = 0;
  bool discardLine_ = false;

  static bool fresh(const Target &target) {
    return target.received && millis() - target.lastPacket <= timeoutMs_;
  }

  static int confirmedType(const Target &target) {
    return target.type >= 0 && target.frames >= confirmFrames_ && fresh(target) ? target.type : -1;
  }

  static bool cleared(const Target &target) {
    return fresh(target) && target.type < 0 && target.frames >= confirmFrames_;
  }

 public:
  static const char *name(int type) {
    switch (type) {
      case 0: return "STABLE";
      case 1: return "UNHARMED";
      case 2: return "HARMED";
      case 3: return "PHI (harmed)";
      case 4: return "PSI (stable)";
      case 5: return "OMEGA (unharmed)";
      default: return "no target";
    }
  }

 private:
  // Existing CSV packet: type,stable_ms,x,y. -1 clears rings, -2 clears PSO.
  void parseLine(unsigned long now) {
    const char *field = line_;
    long values[4];
    for (int i = 0; i < 4; ++i) {
      char *end;
      errno = 0;
      values[i] = strtol(field, &end, 10);
      if (end == field || errno == ERANGE || *end != (i == 3 ? '\0' : ',')) return;
      field = end + 1;
    }

    const long type = values[0];
    const long age = values[1];
    const long x = values[2];
    const long y = values[3];
    if (type < -2 || type > 5) return;
    if (type < 0) {
      if (age != 0 || x != -1 || y != -1) return;
    } else if (age < 0 || x < 0 || x >= coordinateLimit_ || y < 0 || y >= coordinateLimit_) {
      return;
    }

    Target &target = (type == -2 || type >= 3) ? letter_ : ring_;
    if (!target.received || now - target.lastPacket > timeoutMs_) target = Target{};
    if (target.type != type) target.frames = 0;
    if (target.frames < confirmFrames_) ++target.frames;
    target.type = type;
    target.age = age;
    target.x = x;
    target.y = y;
    target.lastPacket = now;
    target.received = true;
    ++acceptedPackets_;
    if (type < 0 && target.frames == confirmFrames_) target.reportedType = -1;
  }

  static void printTarget(const Target &target, const char *label, Print &output) {
    if (!fresh(target)) return;
    if (target.type < 0) {
      output.printf("%s: linked, no target.\n", label);
      return;
    }
    output.printf("%s: %s | x=%d y=%d | stable %lu ms | frames %u/%u\n",
                  label, name(target.type), target.x, target.y, target.age,
                  unsigned(target.frames), unsigned(confirmFrames_));
  }

  static void report(Target &target, const char *label, Print &output) {
    if (confirmedType(target) < 0 || target.type == target.reportedType) return;
    target.reportedType = target.type;
    printTarget(target, label, output);
  }
};

