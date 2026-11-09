# Debug the maze robot one step at a time

Open Serial Monitor at **115200 baud**. When something goes wrong, start with
the earliest step that fails. Change one thing, then repeat the same test.

## Step 1 — Check the wiring

Open `Hardware.h`. The pin numbers are at the top. The ESP32's I²C wires are
**GPIO21 for SDA** and **GPIO22 for SCL**. All grounds join together. Check
these labels against the [wiring table](README.md).

The TCA's channel numbers are separate from the ESP32 pin numbers. On the
TCA, the left, right, and front APDS sensors use **CH0, CH1, and CH7**. Color
uses **CH2**, the left encoder uses **CH5**, and the right encoder uses **CH4**.
The gyro connects directly to the main I²C wires.

## Step 2 — Start the board

Open `MAZESOLVER.ino` with `Hardware.h` in the same folder. Select **ESP32 Dev
Module**, upload, and press EN/Reset. Leave motor power off for this and the
next step. Keep the robot still while it calibrates the gyro.

- **`MPU6050 has no member named resetAngleZ`** means the library needs the
  original [heading reset helper](README.md#mpu-library-setup).
- **`TCA9548A at 0x70 did not respond`** means to check the TCA power, ground,
  GPIO21/22, and address jumpers before looking at an individual sensor.
- **`0x39 is on the main bus`** means to check where the AS7341 and APDS wires
  go. Each of these sensors needs its own TCA channel.

## Step 3 — Test the sensors

Look for three APDS `READY` messages, `Gyro calibrated.`, and `AS7341 CH2 ready`.
The main sketch identifies the IMU by model and address. A typical MPU6050
prints `IMU MPU6050 at 0x68, ID=0x68.` It verifies the gyro and accelerometer
ranges before calibrating both sensors. Keep the robot still until
`Gyro calibrated.` appears. If startup fails, look for the specific
`IMU ERROR` line above it.
If one part is unclear, upload its test sketch by itself. Re-upload the main
sketch after the sensor passes.

| Part | What to try | What to look for |
| --- | --- | --- |
| [Three APDS wall sensors](../../tests/APDS9960_All_Test/APDS9960_All_Test.ino) | Move a hand toward one sensor at a time. | The matching left, right, or front count changes. |
| [TCA and gyro](../../tests/TCA_MPU_Test/TCA_MPU_Test.ino) | Leave the robot still at startup, then turn it by hand. | The TCA control check passes and the Z gyro reading changes when it turns. |
| [Wheel encoders](../../tests/AS5600_TCA9548A_Test/AS5600_TCA9548A_Test.ino) | Turn one wheel at a time by hand. | Its counts change and the magnet status is `OK`. |
| [Floor color](../../tests/AS7341_Color_Test/AS7341_Color_Test.ino) | Hold two different surfaces at the same distance. | CH2 is detected and the light and readings respond. |

If the main sketch stops with a **`NAV FAULT`** line, copy that whole line.
For example, `CH7=FAIL/20ms` means the front APDS read path failed after 20 ms.
Check that sensor and its TCA channel. If the APDS and IMU entries all say
`OK` but `cycle` is over 120 ms, find the slowest read on that line. An encoder
can also take time on the shared bus. `CHECK` next to an encoder on a normal
status line points to a read or magnet issue. Use the encoder test to tell
which one.

## Step 4 — Check start and stop with the wheels lifted

Re-upload the main sketch and wait for the gyro to calibrate. Lift the wheels
before connecting motor power. Send `s` in Serial Monitor, then send `x` and
check for `STOP: serial command` and `PWM 0/0`. The onboard BOOT button also
starts or stops the robot after startup. Once this works, try the left-then-right
hand start sequence printed in Serial Monitor.

## Step 5 — Check one movement at a time

If a wheel turns the wrong way, use the [motor test](../../tests/Motor_Test/Motor_Test.ino)
with the wheels lifted. That sketch starts three seconds after reset. Watch
the wheel named in Serial Monitor. Switch off power before changing wires.

With the robot on the floor and a clear area around it, test a front wall,
a left opening, and a dead end separately. `TURN TIMEOUT: 14.3 of 90 degrees`
means the gyro measured 14.3° during that pivot. Check whether both wheels
actually turned in opposite directions and whether the gyro's Z axis is
perpendicular to the floor. `CORNER_ADVANCE` and `CORNER_REVERSE` use the
encoders, so use the wheel test if either step stops with an encoder error.

## Step 6 — Follow the code while the robot runs

Start at `loop()` in `MAZESOLVER.ino`, then read `followLeftWall()`. It has
three choices in order: a left opening, a wall in front, or straight driving.
Match the choice with `FOLLOW`, `CORNER_ADVANCE`, `PAUSE`, `TURN`, or
`CORNER_REVERSE` in Serial Monitor. Look at Step 5 of the sketch only when the
robot chooses the right action but performs it incorrectly.

When reporting a problem, include the step number, what you expected to
happen, what the robot did, and the relevant Serial Monitor lines. If you
change a tuning value, write down its old value and the result of the retest.
