# Main maze-solver firmware

Open `MAZESOLVER.ino` with `Hardware.h` in the same sketch folder. In Arduino
IDE, select **ESP32 Dev Module** and the robot's USB port. Install **Adafruit
AS7341** and **Adafruit BusIO** if needed. Upload and open Serial Monitor at
**115200 baud**.

| Device | Connection |
| --- | --- |
| I²C bus | GPIO21 SDA and GPIO22 SCL |
| TCA9548A | Main bus at `0x70` |
| MPU6500 or MPU6050 | Main bus at `0x68` or `0x69`, with the Z axis perpendicular to the floor |
| Left, right, front APDS-9960 | TCA channels 0, 1, 7 |
| AS7341 | TCA channel 2, illumination LED at 4 mA |
| Left and right AS5600 | TCA channels 5 and 4 |
| TB6612 left IN1 / IN2 / PWM | GPIO25 / GPIO26 / GPIO27 |
| TB6612 right IN1 / IN2 / PWM | GPIO18 / GPIO19 / GPIO23 |
| TB6612 STBY | GPIO33 |

## First run

1. Leave motor power off while checking sensors. Keep the robot still at reset
   for gyro calibration. Check for three APDS `READY` lines,
   `Gyro calibrated.`, and `AS7341 CH2 ready`. Motors start in standby.
2. While stopped, wave a hand past each APDS and check the corresponding `L`,
   `R`, and `F` readings. Turn the wheels by hand to check encoder counts and
   magnet status. Hold colored surfaces under the AS7341 and check the ten
   readings in its serial output.
3. Lift the wheels, reset the ESP32, wait for calibration, then connect motor
   power. Press BOOT **after startup** or send `s` to start. You can also show a
   hand to the left APDS, remove it, then show and remove a hand at the right
   APDS. Use BOOT or `x` to stop. Check drive direction and stop before floor
   testing.
4. On the floor, start alongside a straight left wall. Test a front obstruction,
   a left opening, and a dead end individually before running a longer route.
   Use `FOLLOW`, `CORNER_ADVANCE`, and `TURN` in Serial Monitor to see the
   current decision.

## Navigation and tuning

The firmware follows the left-hand rule continuously. It uses the original
APDS smoothing and proportional/derivative steering gains with a starting
PWM of 70. A front wall triggers a left turn if the left side is open, a
right turn if the right side is open, or a U-turn at a dead end. The gyro
measures the turn angle. At a detected left opening, the robot uses the two
encoders to clear the corner before turning. This local maneuver starts with
the original 220 mm offset and can end earlier at a front wall.

The starting constants in `MAZESOLVER.ino` include the front threshold of 40,
side-open threshold of 9, side-wall threshold of 25, and side steering target
of 50. These are proximity counts. Validate them against actual wall surfaces
and sensor angles. Check the corner offset and the 90° and 180° turns on the
assembled robot before running a complete maze.

If a turn times out after 3 seconds for 90° or 5 seconds for 180°, the sketch
logs the angle reached, ends the pivot, and rechecks the walls after a 150 ms
settle. Wall following then resumes. During a run, the sketch clears APDS
saturation flags and shows them as `sat=left/right/front` in Serial Monitor.
Steering skips saturated side readings and treats a saturated front sensor as
a blocked path. Navigation stops on an APDS I²C failure, an IMU failure, or a
corner clearance timeout. Corner clearance also requires two usable encoder
magnets.
The AS7341 prints while stopped so its library cannot delay motor control.
Use the [standalone color calibration test](../../README.md#as7341-color-sensor-test)
to capture black and white references and compare your floor materials. The main
firmware reports raw readings while stopped.
