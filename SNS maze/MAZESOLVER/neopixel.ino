void initRGB() {
  strip.begin();             // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();              // Turn OFF all pixels ASAP
  strip.setBrightness(255);  // Set BRIGHTNESS to about 1/5 (max = 255)
}

void showColor(uint32_t color){
  strip.clear();
  strip.setPixelColor(0, color);
  strip.setPixelColor(1, color);
  strip.show();
}