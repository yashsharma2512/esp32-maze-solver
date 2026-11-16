// Nicla Vision sends separate ring and PSO packets over UART1.
// The shared reader validates frames and requires three matching detections.
void serviceCameraTargets() {
  if (!cameraArmed) return;
  niclaVision.service(Serial1, SerialDebug);
  if (rescueSequence.active()) return;
  updateFloorFlash();
  if (floorFlashActive) return;
  targetPixels.showTargets(strip, niclaVision.confirmedRing(), niclaVision.confirmedLetter());
}

bool serviceRescueTarget() {
  if (!cameraArmed) return false;
  serviceCameraTargets();
  if (floorFlashActive) return false;
  return rescueSequence.handle(niclaVision, targetPixels, strip, SerialDebug);
}
