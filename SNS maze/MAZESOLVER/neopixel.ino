void initRGB() {
  strip.begin();
  strip.show();
  strip.setBrightness(255);
}

void showColor(uint32_t color) {
  targetPixels.showStatus(strip, color);
}
