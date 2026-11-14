constexpr uint32_t FLOOR_FLASH_MS = 300;
bool floorDetectionReady = false;
bool floorFlashActive = false;
uint32_t floorFlashStarted = 0;
uint32_t colorBeforeFlash = 0;

void startFloorDetection() {
  floorDetectionReady = colorReady && loadFloorColorReferences();
  if (floorDetectionReady) SerialDebug.println("Floor color detection ready.");
  else SerialDebug.println("Floor color detection needs the AS7341, a floor or white reference, and a tile reference.");
}

void updateFloorFlash() {
  if (floorFlashActive && millis() - floorFlashStarted >= FLOOR_FLASH_MS) {
    showColor(colorBeforeFlash);
    floorFlashActive = false;
  }
}

void flashFloorColor(uint32_t color) {
  if (!floorFlashActive) colorBeforeFlash = strip.getPixelColor(0);
  showColor(color);
  floorFlashStarted = millis();
  floorFlashActive = true;
}

// True means the black-tile U-turn changed direction. Resume from a fresh maze decision.
bool handleFloorColor() {
  updateFloorFlash();
  if (!floorDetectionReady) return false;

  static const char *lastTile = nullptr;
  static const char *candidate = nullptr;
  static uint32_t lastSampleAt = 0;
  uint16_t bands[COLOR_BAND_COUNT];
  const ColorSampleStatus sample = pollColorSample(bands);
  if (sample == ColorSampleStatus::Waiting) return false;
  if (sample == ColorSampleStatus::Failed) {
    candidate = nullptr;
    return false;
  }

  const char *tile = matchFloorColor(bands);
  const uint32_t now = millis();
  const bool confirmed = tile && tile == candidate && now - lastSampleAt <= 300;
  candidate = tile;
  lastSampleAt = now;
  if (!confirmed || tile == lastTile) return false;
  lastTile = tile;

  if (strcmp(tile, "black") == 0) {
    SerialDebug.println("FLOOR BLACK: U-turn");
    moveMotor(0, 0);
    turnBot(90, LEFT);
    delayWithSensorUpdate(100);
    turnBot(90, LEFT);
    if (motorStopRequested()) {
      lastTile = nullptr;
      return true;
    }
    updateWalls(false);
    pe_move = 0;
    return true;
  }
  if (strcmp(tile, "blue") == 0) {
    SerialDebug.println("FLOOR BLUE: puddle, waiting 5 seconds");
    moveMotor(0, 0);
    delayWithSensorUpdate(5000);
    updateFloorFlash();
    flashFloorColor(strip.Color(0, 0, 100));
    delayWithSensorUpdate(FLOOR_FLASH_MS);
    updateFloorFlash();
  } else if (strcmp(tile, "silver") == 0) {
    SerialDebug.println("FLOOR SILVER");
    flashFloorColor(strip.Color(80, 80, 80));
  }
  return false;
}
