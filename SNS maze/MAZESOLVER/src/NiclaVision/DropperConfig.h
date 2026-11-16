#pragma once

// The servo GPIO is selected by config/config_lolins2.h.
constexpr int DROPPER_HOME_DEG = 0;
constexpr int DROPPER_RELEASE_DEG = 90;  // Set after the one-kit calibration test.
constexpr unsigned long DROPPER_TRAVEL_MS = 400;

static_assert(DROPPER_HOME_DEG >= 0 && DROPPER_HOME_DEG <= 180, "Invalid dropper home angle");
static_assert(DROPPER_RELEASE_DEG == -1 ||
                  (DROPPER_RELEASE_DEG >= 0 && DROPPER_RELEASE_DEG <= 180 &&
                   DROPPER_RELEASE_DEG != DROPPER_HOME_DEG),
              "The release angle must be -1 or a valid angle different from home");

