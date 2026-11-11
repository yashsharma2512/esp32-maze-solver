// AS7341 calibration. Save named samples while the motors are stopped.
#include <EEPROM.h>

constexpr size_t MAX_COLOR_REFERENCES = 16;
constexpr size_t COLOR_NAME_SIZE = 16;  // 15 letters plus the string terminator.
constexpr int COLOR_CAPTURE_SAMPLES = 5;
constexpr uint32_t COLOR_STORAGE_FORMAT = 0x434F4C01;  // Color table, version 1.

struct ColorReference {
  char name[COLOR_NAME_SIZE];
  uint16_t bands[COLOR_BAND_COUNT];
};

struct ColorTable {
  uint32_t format = COLOR_STORAGE_FORMAT;
  ColorReference references[MAX_COLOR_REFERENCES] = {};
};

EEPROMClass colorEEPROM("maze_colors");
ColorTable colorTable;
bool colorStorageReady = false;
bool colorCalibrationActive = false;

// Arduino's generated declarations may appear before the ColorReference type.
bool storeColorReference(int slot, const ColorReference &reference);

// 1. Open the EEPROM table and show the available commands.
bool validColorName(const char *name) {
  const size_t length = strlen(name);
  if (length == 0 || length >= COLOR_NAME_SIZE) return false;
  return strspn(name, "abcdefghijklmnopqrstuvwxyz0123456789_-") == length;
}

void loadColorReferences() {
  colorStorageReady = colorEEPROM.begin(sizeof(colorTable));
  if (!colorStorageReady) {
    SerialDebug.println("COLOR ERROR: EEPROM could not be opened. Exit and try c again.");
    return;
  }

  colorEEPROM.get(0, colorTable);
  if (colorTable.format != COLOR_STORAGE_FORMAT) {
    colorTable = ColorTable{};
    return;
  }
  for (auto &reference : colorTable.references) {
    if (memchr(reference.name, '\0', sizeof(reference.name)) == nullptr ||
        !validColorName(reference.name)) {
      reference = ColorReference{};
    }
  }
}

void printColorHelp() {
  SerialDebug.println("Color commands (send each with a newline):");
  SerialDebug.println("  save red    Capture and save a reference. Use any lowercase name.");
  SerialDebug.println("  list        Show the saved names and readings.");
  SerialDebug.println("  read        Show the raw bands and closest saved reference.");
  SerialDebug.println("  delete red  Remove one saved reference.");
  SerialDebug.println("  exit        Return to the hand-gesture startup.");
  SerialDebug.println("  help        Show these commands.");
}

void beginColorCalibration() {
  colorCalibrationActive = true;
  if (!colorStorageReady) loadColorReferences();
  SerialDebug.println("COLOR CALIBRATION: robot stopped.");
  printColorHelp();
}

// 2. Find references and capture five readings from a single surface.
int findColorReference(const char *name) {
  for (size_t i = 0; i < MAX_COLOR_REFERENCES; ++i) {
    if (strcmp(colorTable.references[i].name, name) == 0) return int(i);
  }
  return -1;
}

bool captureColorAverage(uint16_t *average) {
  SerialDebug.println("Hold the target still. Averaging 5 readings.");
  uint32_t totals[COLOR_BAND_COUNT] = {};
  for (int sample = 0; sample < COLOR_CAPTURE_SAMPLES; ++sample) {
    uint16_t bands[COLOR_BAND_COUNT];
    if (!readColorSample(bands)) {
      SerialDebug.printf("COLOR ERROR: AS7341 read failed. Check startup and TCA CH%u.\n", unsigned(COLOR_CH));
      return false;
    }
    for (size_t i = 0; i < COLOR_BAND_COUNT; ++i) totals[i] += bands[i];
  }
  for (size_t i = 0; i < COLOR_BAND_COUNT; ++i) {
    average[i] = (totals[i] + COLOR_CAPTURE_SAMPLES / 2) / COLOR_CAPTURE_SAMPLES;
  }
  return true;
}

// 3. Update flash only when asked to save or delete a reference.
bool storeColorReference(int slot, const ColorReference &reference) {
  const ColorReference previous = colorTable.references[slot];
  colorTable.references[slot] = reference;
  colorEEPROM.put(0, colorTable);
  if (colorEEPROM.commit()) return true;

  colorTable.references[slot] = previous;
  colorEEPROM.put(0, colorTable);
  SerialDebug.println("COLOR ERROR: EEPROM write failed. The previous reference is still in use.");
  return false;
}

void saveColorReference(const char *name) {
  int slot = findColorReference(name);
  if (slot < 0) slot = findColorReference("");
  if (slot < 0) {
    SerialDebug.println("COLOR: all 16 reference slots are used. Delete a color or reuse its name.");
    return;
  }

  ColorReference reference{};
  if (!captureColorAverage(reference.bands)) return;
  strcpy(reference.name, name);
  if (storeColorReference(slot, reference)) SerialDebug.printf("Saved %s to EEPROM.\n", name);
}

void deleteColorReference(const char *name) {
  const int slot = findColorReference(name);
  if (slot < 0) {
    SerialDebug.printf("COLOR: no reference named %s.\n", name);
    return;
  }
  if (storeColorReference(slot, ColorReference{})) SerialDebug.printf("Deleted %s.\n", name);
}

void listColorReferences() {
  SerialDebug.println("Saved colors (F1..F8, Clear, NIR):");
  bool found = false;
  for (const auto &reference : colorTable.references) {
    if (reference.name[0] == '\0') continue;
    found = true;
    SerialDebug.printf("  %s:", reference.name);
    for (uint16_t band : reference.bands) SerialDebug.printf(" %u", unsigned(band));
    SerialDebug.println();
  }
  if (!found) SerialDebug.println("  None. Try save red.");
}

// 4. Compare the current sample with each saved set of ten readings.
int closestColorReference(const uint16_t *sample, float &bestScore, float &secondScore) {
  int closest = -1;
  bestScore = secondScore = INFINITY;
  for (size_t i = 0; i < MAX_COLOR_REFERENCES; ++i) {
    const ColorReference &reference = colorTable.references[i];
    if (reference.name[0] == '\0') continue;
    float score = 0;
    for (size_t band = 0; band < COLOR_BAND_COUNT; ++band) {
      const float difference = float(sample[band]) - reference.bands[band];
      score += difference * difference;
    }
    if (score < bestScore) {
      secondScore = bestScore;
      closest = int(i);
      bestScore = score;
    } else if (score < secondScore) {
      secondScore = score;
    }
  }
  return closest;
}

const char *matchFloorColor(const uint16_t *sample) {
  float bestScore, secondScore;
  const int closest = closestColorReference(sample, bestScore, secondScore);
  if (closest < 0) return nullptr;
  const char *name = colorTable.references[closest].name;
  const bool action = strcmp(name, "black") == 0 || strcmp(name, "blue") == 0 ||
                      strcmp(name, "silver") == 0;
  if (action && secondScore <= 1.5f * bestScore) return nullptr;

  // Keep brightness in the comparison so black and silver remain distinct.
  float referenceEnergy = 0;
  for (uint16_t band : colorTable.references[closest].bands) {
    referenceEnergy += float(band) * band;
  }
  const float tolerance = FLOOR_COLOR_MATCH_TOLERANCE * FLOOR_COLOR_MATCH_TOLERANCE;
  if (bestScore > tolerance * max(referenceEnergy, 100.0f)) return nullptr;
  return action ? name : "floor";
}

void matchColorReference() {
  uint16_t sample[COLOR_BAND_COUNT];
  if (!captureColorAverage(sample)) return;
  SerialDebug.print("AS7341 F1..F8, Clear, NIR:");
  for (uint16_t band : sample) SerialDebug.printf(" %u", unsigned(band));
  SerialDebug.println();
  if (!colorStorageReady) return;

  float bestScore, secondScore;
  const int closest = closestColorReference(sample, bestScore, secondScore);
  if (closest < 0) SerialDebug.println("COLOR: save a reference first.");
  else {
    SerialDebug.printf("Closest saved color: %s\n", colorTable.references[closest].name);
    const char *tile = matchFloorColor(sample);
    SerialDebug.printf("Driving match: %s\n", tile ? tile : "unknown");
  }
}

bool loadFloorColorReferences() {
  if (!colorStorageReady) loadColorReferences();
  if (!colorStorageReady) return false;
  const bool hasFloor = findColorReference("floor") >= 0 || findColorReference("white") >= 0;
  const bool hasTile = findColorReference("black") >= 0 || findColorReference("blue") >= 0 ||
                       findColorReference("silver") >= 0;
  return hasFloor && hasTile;
}

// 5. Read a full command before handling it.
void runColorCommand(char *line) {
  const char *command = strtok(line, " \t");
  if (!command) return;
  const char *name = strtok(nullptr, " \t");

  if (!name && strcmp(command, "exit") == 0) {
    colorCalibrationActive = false;
    SerialDebug.println("Color calibration ended. Show a hand to the left APDS or send start.");
  } else if (!name && strcmp(command, "help") == 0) {
    printColorHelp();
  } else if (!name && strcmp(command, "read") == 0) {
    matchColorReference();
  } else if (!colorStorageReady) {
    SerialDebug.println("COLOR ERROR: EEPROM unavailable. Send exit, then c to retry.");
  } else if (!name && strcmp(command, "list") == 0) {
    listColorReferences();
  } else if (strcmp(command, "save") == 0 || strcmp(command, "delete") == 0) {
    if (!name || strtok(nullptr, " \t") || !validColorName(name)) {
      SerialDebug.println("Use a name of 1 to 15 lowercase letters, digits, underscores, or hyphens.");
    } else if (strcmp(command, "save") == 0) {
      saveColorReference(name);
    } else {
      deleteColorReference(name);
    }
  } else {
    SerialDebug.println("Unknown color command. Send help.");
  }
}

void readColorCommands(bool fromNetwork) {
  char line[40];
  size_t length = 0;
  bool tooLong = false;
  // Use the same input that started calibration.
  while (colorCalibrationActive) {
    const int key = fromNetwork ? SerialDebug.readNetwork() : Serial.read();
    if (key < 0) {
      SerialDebug.handleOta();
      delay(1);
      continue;
    }
    if (key == '\r' || key == '\n') {
      line[length] = '\0';
      if (tooLong) SerialDebug.println("Color command too long. Send help.");
      else if (length > 0) runColorCommand(line);
      length = 0;
      tooLong = false;
    } else if (length + 1 < sizeof(line)) {
      line[length++] = char(key);
    } else {
      tooLong = true;
    }
  }
}

void runColorCalibration(bool fromNetwork) {
  beginColorCalibration();
  readColorCommands(fromNetwork);
  colorCalibrationActive = false;
}
