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

Start at `loop()` in `MAZESOLVER.ino`. It reads the sensors, then calls
`followLeftWall()` when the robot is running. That function reads in movement
order. At a left opening it calls `moveCornerDistance()`, `turn90()`, and
`moveCornerDistance()` again for the backup. Each movement calls
`updateDuringMove()` to check the sensors and stop controls. Adjust the tuning
values at the top of the sketch. `Hardware.h` holds the wiring and device reads.

The firmware follows the left-hand rule continuously. It uses the original
APDS smoothing, proportional/derivative steering, gyro heading correction,
and base PWM of 100. `GYRO_Z_SIGN` assumes the IMU Z axis points up. Use `-1`
if the board is mounted with Z pointing down.

At a left opening the robot advances up to 220 mm, turns 90° left, then
backs up 30 mm. If the front is blocked, it pauses for 200 ms and chooses
the turn from fresh side readings. At a dead end it turns left twice and
reverses for one second at PWM 100. A single front-wall turn also uses the
30 mm backup. The encoder-based moves end early if the front reading crosses
the wall threshold.

The front wall test uses your single raw CH7 reading above 40. A filtered
side reading of 9 or less counts as open for turns. Straight steering uses
the whole-number filtered side count above 25 and a target of 50. Validate
these values against the actual wall surfaces and sensor angles.

The gyro turn controller uses the original gains, PWM limit of 165, and a
1.1-second limit per 90° turn. If it times out before reaching the target,
the sketch logs the angle, pauses for 100 ms, and resumes wall following from
fresh readings. The ESP32 checks motion and sensors every 10 ms and can stop
with BOOT or `x` during a maneuver. Navigation stops on an APDS I²C failure,
an IMU failure, or an encoder maneuver failure. Corner moves require two usable
encoder magnets.
The AS7341 prints while stopped so its library cannot delay motor control.
Use the [standalone color calibration test](../../README.md#as7341-color-sensor-test)
to capture black and white references and compare your floor materials. The main
firmware reports raw readings while stopped.

If the run stops with `navigation sensor unavailable or late`, copy the next
`NAV FAULT` line. It shows the motion phase and the read result and duration for
each APDS channel, the IMU, and both encoders. Any APDS or IMU failure stops the
run. Encoder read failures contribute to the measured cycle time, but by
themselves only stop encoder-based maneuvers. A cycle over 120 ms stops the run
even when each sensor eventually responds.
