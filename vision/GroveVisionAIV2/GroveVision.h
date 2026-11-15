#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Seeed_Arduino_SSCMA.h>
#include <freertos/task.h>

// The Grove V2 runs inference on its own chip. A separate ESP32 task reads
// results over UART2 so inference never pauses the wall-follow control loop.
class GroveVisionReader {
 public:
  enum class Target : uint8_t { None, Phi, Psi, Omega, Unknown };
  enum class State : uint8_t { Idle, Connecting, NoModelLabels, Ready, InferenceError, TaskError };
  struct Event {
    Target target;
    uint8_t classId;
    uint8_t score;
    uint32_t detectedAtMs;
  };

  void begin(int rxPin, int txPin) {
    if (state() != State::Idle) return;
    serial_.setRxBufferSize(2048);
    if (!serial_.setPins(rxPin, txPin)) {
      setState(State::TaskError);
      return;
    }
    setState(State::Connecting);
    if (xTaskCreatePinnedToCore(taskEntry, "Grove V2", 10240, this, 1, nullptr, 0) != pdPASS) {
      setState(State::TaskError);
    }
  }

  State state() {
    portENTER_CRITICAL(&lock_);
    const State value = state_;
    portEXIT_CRITICAL(&lock_);
    return value;
  }

  bool takeEvent(Event &result) {
    portENTER_CRITICAL(&lock_);
    const bool available = eventNumber_ != consumedEventNumber_;
    if (available) {
      result = event_;
      consumedEventNumber_ = eventNumber_;
    }
    portEXIT_CRITICAL(&lock_);
    return available;
  }

  void service(Print &output) {
    const State current = state();
    if (current != printedState_) {
      printedState_ = current;
      switch (current) {
        case State::Connecting: output.println("Grove V2: connecting on UART2."); break;
        case State::NoModelLabels:
          output.println("Grove V2: deploy a model labelled harmed/stable/unharmed/nothing.");
          break;
        case State::Ready: output.println("Grove V2: Greek victim model ready."); break;
        case State::InferenceError: output.println("Grove V2: inference error; retrying."); break;
        case State::TaskError: output.println("Grove V2: UART pins or task unavailable."); break;
        default: break;
      }
    }
    Event event;
    if (!takeEvent(event)) return;
    if (millis() - event.detectedAtMs > 1500) return;
    output.print("GROVE TARGET: ");
    switch (event.target) {
      case Target::Phi: output.print("PHI (harmed)"); break;
      case Target::Psi: output.print("PSI (stable)"); break;
      case Target::Omega: output.print("OMEGA (unharmed)"); break;
      default: return;
    }
    output.print(" | class=");
    output.print(event.classId);
    output.print(" score=");
    output.println(event.score);
  }

 private:
  static constexpr uint8_t minimumScore_ = 75;
  static constexpr uint8_t requiredFrames_ = 3;
  HardwareSerial &serial_ = Serial2;
  SSCMA vision_;
  String labels_[16];
  size_t labelCount_ = 0;
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  State state_ = State::Idle;
  State printedState_ = State::Idle;
  Event event_ = {Target::None, 0, 0, 0};
  uint32_t eventNumber_ = 0;
  uint32_t consumedEventNumber_ = 0;

  static int base64Digit(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  }

  static String decodeBase64(const String &encoded) {
    String decoded;
    decoded.reserve(encoded.length() * 3 / 4);
    uint32_t bits = 0;
    uint8_t bitCount = 0;
    for (size_t i = 0; i < encoded.length(); ++i) {
      if (encoded[i] == '=') break;
      const int digit = base64Digit(encoded[i]);
      if (digit < 0) return String();
      bits = (bits << 6) | uint32_t(digit);
      bitCount += 6;
      if (bitCount >= 8) {
        bitCount -= 8;
        decoded += char((bits >> bitCount) & 0xff);
      }
    }
    return decoded;
  }

  bool readModelLabels() {
    const String json = decodeBase64(vision_.info(false));
    if (json.isEmpty()) return false;
    JsonDocument document;
    if (deserializeJson(document, json)) return false;
    JsonArray classes = document["classes"].as<JsonArray>();
    if (classes.isNull() || classes.size() > 16) return false;
    labelCount_ = 0;
    bool hasGreekTarget = false;
    for (JsonVariant entry : classes) {
      labels_[labelCount_++] = entry.as<String>();
      const Target target = targetForId(labelCount_ - 1);
      if (target == Target::Phi || target == Target::Psi || target == Target::Omega) {
        hasGreekTarget = true;
      }
    }
    return hasGreekTarget;
  }

  Target targetForId(uint8_t id) const {
    if (id >= labelCount_) return Target::Unknown;
    String label = labels_[id];
    label.trim();
    label.toLowerCase();
    if (label == "phi" || label == "harmed" || label == "Φ" || label == "φ") return Target::Phi;
    if (label == "psi" || label == "stable" || label == "Ψ" || label == "ψ") return Target::Psi;
    if (label == "omega" || label == "unharmed" || label == "Ω" || label == "ω") return Target::Omega;
    if (label == "nothing" || label == "none" || label == "background") return Target::None;
    return Target::Unknown;
  }

  void setState(State value) {
    portENTER_CRITICAL(&lock_);
    state_ = value;
    portEXIT_CRITICAL(&lock_);
  }

  void publish(Target target, uint8_t classId, uint8_t score) {
    portENTER_CRITICAL(&lock_);
    event_ = {target, classId, score, millis()};
    ++eventNumber_;
    portEXIT_CRITICAL(&lock_);
  }

  static void taskEntry(void *context) {
    static_cast<GroveVisionReader *>(context)->run();
  }

  void run() {
    for (;;) {
      if (!vision_.begin(&serial_)) {
        setState(State::Connecting);
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
      while (!readModelLabels()) {
        setState(State::NoModelLabels);
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
      setState(State::Ready);

      Target candidate = Target::None;
      Target lastReported = Target::None;
      uint8_t matchingFrames = 0;
      uint8_t absentFrames = 0;
      uint8_t errors = 0;
      for (;;) {
        if (vision_.invoke(1, false, false) != 0) {
          candidate = Target::None;
          matchingFrames = 0;
          if (++errors >= 5) {
            setState(State::InferenceError);
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(250));
          continue;
        }
        errors = 0;
        uint8_t bestId = 0;
        uint8_t bestScore = 0;
        bool found = false;
        for (const auto &entry : vision_.classes()) {
          if (!found || entry.score > bestScore) {
            bestId = entry.target;
            bestScore = entry.score;
            found = true;
          }
        }
        for (const auto &entry : vision_.boxes()) {
          if (!found || entry.score > bestScore) {
            bestId = entry.target;
            bestScore = entry.score;
            found = true;
          }
        }
        const Target target = found && bestScore >= minimumScore_
                                  ? targetForId(bestId) : Target::None;
        if (target == Target::None || target == Target::Unknown) {
          candidate = Target::None;
          matchingFrames = 0;
          if (absentFrames < requiredFrames_) ++absentFrames;
          if (absentFrames >= requiredFrames_) lastReported = Target::None;
        } else {
          absentFrames = 0;
          if (target == candidate) {
            if (matchingFrames < requiredFrames_) ++matchingFrames;
          } else {
            candidate = target;
            matchingFrames = 1;
          }
          if (matchingFrames == requiredFrames_ && target != lastReported) {
            publish(target, bestId, bestScore);
            lastReported = target;
          }
        }
        setState(State::Ready);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
};
