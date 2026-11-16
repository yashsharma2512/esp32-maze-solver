# Dropper servo calibration

Open [Dropper_Calibration.ino](Dropper_Calibration.ino) with the repository
folders intact and select **ESP32 Dev Module**. This test uses
[DropperConfig.h](../../vision/NiclaVision/DropperConfig.h) for GPIO2. It keeps
TB6612 standby GPIO33 low. Connect servo signal to GPIO2, provide suitable
servo power, and connect servo ground to ESP32 ground.

With the hopper empty, open Serial Monitor at 115200 baud with Newline. Send
an angle such as `0` or `90`. The sketch reports the commanded angle. Send
`off` to stop the servo PWM. Move in small steps while finding positions for
parked and one-kit release, then check a release-and-return cycle.

Set the measured angles in the `src/NiclaVision/DropperConfig.h` inside the
robot sketch you will upload. Each sketch has its own copy. Re-upload that
maze sketch after calibration. A commanded angle is not proof that a kit
actually left the hopper.
