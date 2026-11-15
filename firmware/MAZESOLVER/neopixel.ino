void initRGB() {
  strip.begin();             // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();              // Turn OFF all pixels ASAP
  strip.setBrightness(255);  // Set BRIGHTNESS to about 1/5 (max = 255)
}

void showColor(uint32_t color) {
  targetPixels.showStatus(strip, color);
}

void serviceCameraTargets() {
  niclaVision.service(Serial1, SerialDebug);
  if (rescueSequence.active()) return;
  targetPixels.showTargets(strip, niclaVision.confirmedRing(), niclaVision.confirmedLetter());
}

bool serviceRescueTarget() {
  serviceCameraTargets();
  return rescueSequence.handle(niclaVision, targetPixels, strip, SerialDebug);
}