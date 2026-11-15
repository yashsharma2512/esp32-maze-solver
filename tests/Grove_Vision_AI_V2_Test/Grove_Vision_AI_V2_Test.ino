#include <Arduino.h>
#include <ArduinoJson.h>
#include <Seeed_Arduino_SSCMA.h>

// UART2 keeps the Nicla on UART1 GPIO16/17 for future cognitive targets.
constexpr int VISION_RX_PIN = 34;  // Grove V2 D6/TX -> ESP32 RX
constexpr int VISION_TX_PIN = 14;  // ESP32 TX -> Grove V2 D7/RX
constexpr int MOTOR_STBY_PIN = 33;
constexpr uint8_t MIN_SCORE = 75;  // SSCMA confidence is 0..100.
constexpr uint8_t CONFIRM_FRAMES = 3;

SSCMA vision;
bool visionReady = false;
String modelLabels[16];
size_t modelLabelCount = 0;

enum class Target { None, Phi, Psi, Omega, Unmapped };
Target previousTarget = Target::None;
uint8_t matchingFrames = 0;

int base64Digit(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

String decodeBase64(const String &encoded) {
  String decoded;
  decoded.reserve(encoded.length() * 3 / 4);
  uint32_t bits = 0;
  uint8_t bitCount = 0;
  for (size_t i = 0; i < encoded.length(); ++i) {
    const char c = encoded[i];
    if (c == '=') break;
    const int digit = base64Digit(c);
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
  const String json = decodeBase64(vision.info());
  if (json.isEmpty()) return false;
  JsonDocument document;
  if (deserializeJson(document, json)) return false;
  JsonArray classes = document["classes"].as<JsonArray>();
  if (classes.isNull() || classes.size() > 16) return false;
  modelLabelCount = 0;
  for (JsonVariant label : classes) modelLabels[modelLabelCount++] = label.as<String>();
  return modelLabelCount > 0;
}

Target targetForId(uint8_t id) {
  if (id >= modelLabelCount) return Target::Unmapped;
  String label = modelLabels[id];
  label.trim();
  label.toLowerCase();
  if (label == "phi" || label == "harmed" || label == "Φ" || label == "φ") return Target::Phi;
  if (label == "psi" || label == "stable" || label == "Ψ" || label == "ψ") return Target::Psi;
  if (label == "omega" || label == "unharmed" || label == "Ω" || label == "ω") return Target::Omega;
  if (label == "nothing" || label == "none" || label == "background") return Target::None;
  return Target::Unmapped;
}

const char *targetName(Target target) {
  switch (target) {
    case Target::Phi: return "PHI (harmed)";
    case Target::Psi: return "PSI (stable)";
    case Target::Omega: return "OMEGA (unharmed)";
    case Target::Unmapped: return "UNMAPPED CLASS";
    default: return "NO TARGET";
  }
}

void setup() {
  // The test never moves the robot, even if a motor command was left active.
  pinMode(MOTOR_STBY_PIN, OUTPUT);
  digitalWrite(MOTOR_STBY_PIN, LOW);
  Serial.begin(115200);
  delay(300);
  Serial.println("Grove Vision AI V2: Phi/Psi/Omega test");

  Serial2.setRxBufferSize(2048);
  // GPIO34 has no internal pull-up; the Grove TX line drives UART idle high.
  if (!Serial2.enableRxInternalPull(false) ||
      !Serial2.setPins(VISION_RX_PIN, VISION_TX_PIN)) {
    Serial.println("ERROR: could not assign UART pins 34/14.");
    return;
  }
  while (!vision.begin(&Serial2)) {
    Serial.println("Grove V2 not responding. Check 5V, GND, D6/TX, D7/RX.");
    delay(2000);
  }
  visionReady = true;
  Serial.println("Grove V2 connected at 921600 baud.");

  if (!readModelLabels()) {
    Serial.println("No model labels available. Load the trained model on the Grove V2.");
    return;
  }
  for (size_t id = 0; id < modelLabelCount; ++id) {
    Serial.printf("Model class %u = %s\n", unsigned(id), modelLabels[id].c_str());
  }
}

void loop() {
  if (!visionReady) {
    delay(1000);
    return;
  }
  // No image transfer: request inference results only.
  const int result = vision.invoke(1, false, false);
  if (result != 0) {
    Serial.printf("Inference error: %d\n", result);
    previousTarget = Target::None;
    matchingFrames = 0;
    delay(500);
    return;
  }

  int bestId = -1;
  uint8_t bestScore = 0;
  for (const auto &entry : vision.classes()) {
    Serial.printf("class id=%u score=%u\n", unsigned(entry.target), unsigned(entry.score));
    if (entry.score > bestScore) {
      bestId = entry.target;
      bestScore = entry.score;
    }
  }
  for (const auto &entry : vision.boxes()) {
    Serial.printf("box id=%u score=%u x=%u y=%u w=%u h=%u\n",
                  unsigned(entry.target), unsigned(entry.score),
                  unsigned(entry.x), unsigned(entry.y), unsigned(entry.w), unsigned(entry.h));
    if (entry.score > bestScore) {
      bestId = entry.target;
      bestScore = entry.score;
    }
  }

  const Target target = bestId >= 0 && bestScore >= MIN_SCORE
                            ? targetForId(uint8_t(bestId)) : Target::None;
  if (target == Target::None || target == Target::Unmapped) {
    previousTarget = Target::None;
    matchingFrames = 0;
  } else if (target == previousTarget) {
    if (matchingFrames < CONFIRM_FRAMES) ++matchingFrames;
  } else {
    previousTarget = target;
    matchingFrames = 1;
  }

  Serial.printf("Result: %s | id=%d score=%u | confirmed=%u/%u\n",
                targetName(target), bestId, unsigned(bestScore),
                unsigned(matchingFrames), unsigned(CONFIRM_FRAMES));
  delay(250);
}
