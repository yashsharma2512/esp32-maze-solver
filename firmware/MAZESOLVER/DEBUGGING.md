# Debug the maze robot one step at a time

Open Serial Monitor at **115200 baud**. When something goes wrong, start with
the earliest step that fails. Change one thing, then repeat the same test.

## Step 1 — Check the wiring

Open [Settings.h](Settings.h). The pin numbers are at the top. The ESP32's
I²C wires are **GPIO21 for SDA** and **GPIO22 for SCL**. All grounds join
together. Check these labels against the [wiring table](README.md).

The TCA's channel numbers are separate from the ESP32 pin numbers. On the
TCA, the left, right, and front APDS sensors use **CH0, CH1, and CH7**. Color
uses **CH2**, the left encoder uses **CH5**, and the right encoder uses **CH4**.
The gyro connects directly to the main I²C wires.

## Step 2 — Start the board

Open `MAZESOLVER.ino` with all its tabs in the same folder. Select **ESP32 Dev
Module**, upload, and press EN/Reset. Leave motor power off for this and the
next step. Keep the robot still while it calibrates the gyro. The startup check
is `startBoard()` in [02_Startup.ino](02_Startup.ino).

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
If one part is unclear, upload its test sketch by itself to check the hardware.
Re-upload the main sketch to check the same reading through `readSensors()`
in [03_Sensors.ino](03_Sensors.ino).

| Main sketch helper | Standalone test | What to check |
| --- | --- | --- |
| `sampleAPDS()` in [WallSensors.ino](WallSensors.ino) | [Three APDS wall sensors](../../tests/APDS9960_All_Test/APDS9960_All_Test.ino) | Move a hand toward one sensor at a time. The matching left, right, or front count changes. |
| `selectMux()` in [I2C.ino](I2C.ino) and `sampleIMU()` in [Gyro.ino](Gyro.ino) | [TCA and gyro](../../tests/TCA_MPU_Test/TCA_MPU_Test.ino) | The TCA control check passes. Let the gyro calibrate while still, then turn the robot by hand and watch the gyro reading. |
| `sampleEncoder()` in [Encoders.ino](Encoders.ino) | [Wheel encoders](../../tests/AS5600_TCA9548A_Test/AS5600_TCA9548A_Test.ino) | Turn one wheel by hand. Its counts change and its magnet status is `OK`. |
| `sampleColor()` in [ColorSensor.ino](ColorSensor.ino) | [Floor color](../../tests/AS7341_Color_Test/AS7341_Color_Test.ino) | The LED lights up. Hold two surfaces at the same distance and compare readings. |

To test one main-sketch read directly, copy the whole `MAZESOLVER` folder
to a different location with the same folder name. Leave motor power off.
Keep `setup()` so it initializes the devices, then replace `loop()` in the
copy with this example for the left APDS.

```cpp
void loop() {
  if (busReady && sampleAPDS(leftAPDS)) Serial.println(leftAPDS.raw);
  else Serial.println("Left APDS read failed");
  delay(250);
}
```

Check that the count rises as you move a hand closer. Change both instances
of `leftAPDS` to `rightAPDS` or `frontAPDS` to check one other sensor.

For the AS7341, use [the calibration commands](README.md#save-and-test-floor-colors)
in the main sketch. After `save red` succeeds, reset the ESP32 and use
`c` then `list` to confirm the reference was saved. If a command appears to
do nothing, check that Serial Monitor is set to **Newline**.

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
hand start sequence printed in Serial Monitor. Look in [04_Controls.ino](04_Controls.ino)
for `readControls()`, `checkHandStart()`, and `stopRun()`.

## Step 5 — Check one movement at a time

If a wheel turns the wrong way, use the [motor test](../../tests/Motor_Test/Motor_Test.ino)
with the wheels lifted. That sketch starts three seconds after reset. Watch
the wheel named in Serial Monitor. Switch off power before changing wires.
The main sketch's motor commands are in [Motors.ino](Motors.ino).

To run one actual movement helper by itself, make a copy of the whole
`MAZESOLVER` folder in another location, keeping the same folder name. In the
copy, replace the `if (running)` block in `loop()` with this one-turn test.

```cpp
  if (running) {
    turn90(TURN_LEFT);
    if (running) stopRun("bench test complete");
    return;
  }
```

Lift the wheels, upload the copy, let the gyro calibrate, then send `s`.
Both wheels should spin in opposite directions. The turn may time out on the
stand because the chassis stays still. Send `x` or press BOOT to stop early.
To check encoder-based movement on the stand, clear the front sensor and replace
`turn90(TURN_LEFT);` with `moveCornerDistance(CORNER_ADVANCE_MM);`. Check that
both encoder counts change and the wheels stop when the maneuver ends. Both
helpers live in [05_Movement.ino](05_Movement.ino). Re-upload the original sketch
to return to wall following.

With the robot on the floor and a clear area around it, test a front wall,
a left opening, and a dead end separately. `TURN TIMEOUT: 14.3 of 90 degrees`
means the gyro measured 14.3° during that pivot. Check whether both wheels
actually turned in opposite directions and whether the gyro's Z axis is
perpendicular to the floor. `CORNER_ADVANCE` and `CORNER_REVERSE` use the
encoders, so use the wheel test if either step stops with an encoder error.

## Step 6 — Follow the code while the robot runs

Start at `loop()` in `MAZESOLVER.ino`, then read `followLeftWall()` in
[06_WallFollow.ino](06_WallFollow.ino). It checks for a left opening, checks
for a wall in front, then drives straight if neither applies.
Match the choice with `FOLLOW`, `CORNER_ADVANCE`, `PAUSE`, `TURN`, or
`CORNER_REVERSE` in Serial Monitor. If the robot chooses the right action but
performs it incorrectly, open [05_Movement.ino](05_Movement.ino).

When reporting a problem, include the step number, what you expected to
happen, what the robot did, and the relevant Serial Monitor lines. If you
change a tuning value, write down its old value and the result of the retest.
