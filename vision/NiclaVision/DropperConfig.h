#pragma once

// GPIO2 and 85 degrees were the original sketch's proposed servo settings.
constexpr int DROPPER_PIN = 2;
constexpr int DROPPER_HOME_DEG = 85;
constexpr int DROPPER_RELEASE_DEG = -1;  // Set after the one-kit calibration test.
constexpr unsigned long DROPPER_TRAVEL_MS = 400;

static_assert(DROPPER_HOME_DEG >= 0 && DROPPER_HOME_DEG <= 180, "Invalid dropper home angle");
static_assert(DROPPER_RELEASE_DEG == -1 ||
                  (DROPPER_RELEASE_DEG >= 0 && DROPPER_RELEASE_DEG <= 180 &&
                   DROPPER_RELEASE_DEG != DROPPER_HOME_DEG),
              "The release angle must be -1 or a valid angle different from home");
