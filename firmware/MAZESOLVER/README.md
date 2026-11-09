# Main maze-solver firmware

Use the [step-by-step debugging guide](DEBUGGING.md) to test one part at a
time. Start in [MAZESOLVER.ino](MAZESOLVER.ino) and read `setup()` and `loop()`
from top to bottom. Each helper lives in a separate tab from the main sketch.

Open `MAZESOLVER.ino` in Arduino IDE and keep all the tabs in the same
`MAZESOLVER` folder. Select **ESP32 Dev Module** and the robot's USB port.
In Library Manager, install **Adafruit AS7341**, **Adafruit BusIO**, and
**MPU6050_light** (tested with 1.2.1). Check the [MPU library setup](#mpu-library-setup)
before compiling. Upload and open Serial Monitor at **115200 baud**.

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

## Where to look in the code

| Step | Tab | What it does |
| --- | --- | --- |
| 1 | [Settings.h](Settings.h) | Pins, I²C addresses, and tuning values |
| 2 | [02_Startup.ino](02_Startup.ino) | Starts Serial and checks the I²C bus |
| 3 | [03_Sensors.ino](03_Sensors.ino) | Starts the sensors, takes readings, and reports faults |
| 4 | [04_Controls.ino](04_Controls.ino) | Handles BOOT, Serial, and the hand-gesture start |
| 5 | [05_Movement.ino](05_Movement.ino) | Drives straight, turns, and moves a set distance |
| 6 | [06_WallFollow.ino](06_WallFollow.ino) | Chooses the next move from the wall readings |

The individual hardware tabs are linked in the [sensor and motor checks](DEBUGGING.md).
`Robot.h` declares readings and run state shared by the tabs.
`ColorCalibration.ino` handles color capture, matching, and EEPROM storage.

## MPU library setup

The original sketch uses a small addition to `MPU6050_light` to reset the
heading to zero. If the compiler says `MPU6050` has no member named
`resetAngleZ`, follow these steps.

1. Open `libraries/MPU6050_light/src/MPU6050_light.h` in your Arduino
   sketchbook folder. Find the sketchbook location in Arduino IDE's Preferences.
2. Find `getAngleZ()` in the `public:` section and add this line below it.

   ```cpp
   void resetAngleZ() { angleZ = 0; }
   ```

3. Save the header and compile the sketch again.

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

## Save and test floor colors

1. Stop the robot. Open Serial Monitor at **115200 baud** and choose **Newline**.
2. Send `c` and wait for `COLOR CALIBRATION: robot stopped.` The robot stays
   stopped until you exit calibration.
3. Place the red surface under the AS7341 at the distance it will be from
   the floor. Send `save red` and hold it still until you see
   `Saved red to EEPROM.` Repeat for each surface using its own name, for
   example `save black`, `save white`, `save green`, or `save silver`.
4. Send `list` to see the saved references. Swap in a test surface at the same
   distance and send `read` to see the closest saved reference.
5. Send `exit` to return to the normal start and stop controls. To check
   persistence, reset the board, send `c` again, and send `list`.

Each reference averages five readings of F1 through F8, Clear, and NIR.
The ESP32 can hold 16 references. Names can have up to 15 lowercase letters,
digits, underscores, or hyphens. Saving the same name replaces it. Use
`delete red` to remove one reference or `help` to see all commands.

`read` compares raw band counts, so keep the sensor height and lighting
consistent. It always reports the closest saved sample when at least one
reference exists. Calibrate with the actual tiles you plan to test.

## Navigation and tuning

Start at `loop()` in `MAZESOLVER.ino`. It reads the sensors, then calls
`followLeftWall()` when the robot is running. That function reads in movement
order. At a left opening it calls `moveCornerDistance()`, `turn90()`, and
`moveCornerDistance()` again for the backup. Each movement calls
`updateDuringMove()` to check the sensors and stop controls. The tuning
values and pin assignments are in [Settings.h](Settings.h).

The firmware follows the left-hand rule continuously. It uses the original
APDS smoothing, proportional/derivative steering, gyro heading correction,
and base PWM of 100. `GYRO_Z_SIGN` assumes the IMU Z axis points up. Use `-1`
if the board is mounted with Z pointing down.

Startup applies the original `mpu.begin(3, 0)` settings of ±2,000°/s for the
gyro and ±2 g for the accelerometer. The firmware identifies the MPU6050 or
MPU6500 and verifies both ranges. It uses the original
`mpu.calcOffsets(true, true)` to calibrate the gyro and accelerometer.
`mpu.update()` tracks the heading and `mpu.getAngleZ()` supplies the angle for
straight driving and turns. Use one IMU for navigation. The sketch checks
`0x68` before `0x69`.

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
with BOOT or `x` during a maneuver. The firmware retries a failed APDS read
or IMU communication check once. If a retry also fails, the robot stops.
Encoder reads run in standby and during distance moves. Corner
distance moves require two usable encoders.
The AS7341 prints while stopped so its library cannot delay motor control.
The [standalone color test](../../README.md#as7341-color-sensor-test) can check
the sensor independently. Use the [main sketch's calibration mode](#save-and-test-floor-colors)
to save references to EEPROM.

If the run stops, copy the `STOP` line and any `NAV FAULT` line just below it.
`NAV FAULT` shows the motion phase and each sensor's result and duration.
The firmware stops a run when a read cycle exceeds 120 ms. An encoder entry
marked `SKIP` means that cycle did not sample the encoder.
