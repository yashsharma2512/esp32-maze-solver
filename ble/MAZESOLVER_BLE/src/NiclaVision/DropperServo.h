#pragma once

#include <Arduino.h>
#include "DropperConfig.h"

class DropperServo {
 public:
  bool setAngle(int angle) {
    if (angle < 0 || angle > 180) return false;
    if (!attached_) {
      // Use the second LEDC group on the ESP32, away from the motor PWM.
      attached_ = ledcAttachChannel(DROPPER_PIN, 50, 14, 15);
      if (!attached_) return false;
    }

    // At 50 Hz, a 20 ms period carries a 1 to 2 ms position pulse.
    const uint32_t pulseUs = 1000 + uint32_t(angle) * 1000 / 180;
    const uint32_t duty = (pulseUs * (1UL << 14) + 10000) / 20000;
    return ledcWrite(DROPPER_PIN, duty);
  }

  void off() {
    if (!attached_) return;
    ledcDetach(DROPPER_PIN);
    attached_ = false;
  }

 private:
  bool attached_ = false;
};
