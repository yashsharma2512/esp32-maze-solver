#pragma once

#include <Adafruit_NeoPixel.h>

class NiclaTargetPixels {
 public:
  void showStatus(Adafruit_NeoPixel &strip, uint32_t color) {
    statusColor_ = color;
    showPair(strip, color, color);
  }

  uint32_t statusColor() const { return statusColor_; }

  void showTargets(Adafruit_NeoPixel &strip, int ringType, int letterType) const {
    const uint32_t ringColor = targetColor(ringType);
    const uint32_t letterColor = targetColor(letterType);
    if (ringColor == 0 && letterColor == 0) {
      showPair(strip, statusColor_, statusColor_);
      return;
    }

    // One detection uses both pixels. When both are visible, give each a pixel.
    showPair(strip, ringColor ? ringColor : letterColor, letterColor ? letterColor : ringColor);
  }

 private:
  uint32_t statusColor_ = 0;

  static uint32_t targetColor(int type) {
    switch (type) {
      case 0: return Adafruit_NeoPixel::Color(100, 85, 0);   // Stable ring: yellow
      case 1: return Adafruit_NeoPixel::Color(35, 100, 0);   // Unharmed ring: lime
      case 2: return Adafruit_NeoPixel::Color(100, 0, 0);    // Harmed ring: red
      case 3: return Adafruit_NeoPixel::Color(100, 0, 80);   // Phi: magenta
      case 4: return Adafruit_NeoPixel::Color(0, 80, 100);   // Psi: cyan
      case 5: return Adafruit_NeoPixel::Color(0, 0, 100);    // Omega: blue
      default: return 0;
    }
  }

  static void showPair(Adafruit_NeoPixel &strip, uint32_t first, uint32_t second) {
    if (strip.getPixelColor(0) == first && strip.getPixelColor(1) == second) return;
    strip.setPixelColor(0, first);
    strip.setPixelColor(1, second);
    strip.show();
  }
};
