#pragma once

#include "DropperServo.h"
#include "NiclaVision.h"
#include "TargetPixels.h"

void moveMotor(int left, int right);
void delayWithSensorUpdate(int ms);
bool motorStopRequested();
void requestMotorStop();

class RescueSequence {
 public:
  bool active() const { return active_; }

  bool handle(NiclaVisionReader &vision, NiclaTargetPixels &pixels, Adafruit_NeoPixel &strip, Print &log) {
    // Require a fresh clear scene before the same encounter can use more kits.
    if (handled_) {
      if (vision.targetsCleared()) handled_ = false;
      return false;
    }
    if (motorStopRequested()) return false;
    const int target = vision.rescueTarget();
    if (target < 0) return false;

    handled_ = true;
    active_ = true;
    moveMotor(0, 0);
    pixels.showRescueTarget(strip, target);

    const int kitCount = kitsFor(target);
    log.printf("RESCUE: %s | kits requested: %d\n", NiclaVisionReader::name(target), kitCount);
    if (kitCount > 0) {
      dispense(kitCount, log);
    } else {
      delayWithSensorUpdate(200);  // Give the motors time to stop for a zero-kit target.
    }
    active_ = false;
    return true;
  }

 private:
  DropperServo servo_;
  bool handled_ = false;
  bool active_ = false;

  static int kitsFor(int target) {
    switch (target) {
      case 2: case 3: return 2;  // Harmed ring or Phi.
      case 0: case 4: return 1;  // Stable ring or Psi.
      default: return 0;        // Unharmed ring or Omega.
    }
  }

  void dispense(int kitCount, Print &log) {
    if (DROPPER_RELEASE_DEG < 0) {
      log.println("DROPPER: calibrate the release angle first.");
      return;
    }
    if (!servo_.setAngle(DROPPER_HOME_DEG)) {
      log.println("DROPPER: could not set the parked position.");
      requestMotorStop();
      return;
    }
    delayWithSensorUpdate(DROPPER_TRAVEL_MS);
    for (int kit = 0; kit < kitCount && !motorStopRequested(); ++kit) {
      if (!servo_.setAngle(DROPPER_RELEASE_DEG)) {
        log.println("DROPPER: release pulse failed.");
        requestMotorStop();
        return;
      }
      delayWithSensorUpdate(DROPPER_TRAVEL_MS);
      if (!servo_.setAngle(DROPPER_HOME_DEG)) {
        log.println("DROPPER: return to parked position failed.");
        requestMotorStop();
        return;
      }
      if (motorStopRequested()) break;
      delayWithSensorUpdate(DROPPER_TRAVEL_MS);
      log.printf("DROPPER: cycle %d/%d finished.\n", kit + 1, kitCount);
    }
    if (motorStopRequested()) log.println("DROPPER: dispensing interrupted by stop.");
  }
};
