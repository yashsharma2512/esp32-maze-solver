# SNS maze bench test

Use `NiclaVision/main.py` on the Nicla Vision and
`MAZESOLVER/MAZESOLVER.ino` on the LOLIN S2 Mini. Keep the WiFi maze
sketch's Nicla script separate.

## 1. Check the camera link

With the Nicla running in OpenMV IDE, its Serial Terminal should print
`Nicla SNS UART4 TX ready on ESLOV SDA` and an increasing
`Nicla SNS UART4 TX bytes` count. The ESLOV **SDA_EXT** wire must reach
LOLIN **GPIO12**; connect the grounds. LOLIN GPIO16/17 are the TCA sensor
I²C bus and do not receive this UART stream.

## 2. Arm on a bench

Raise the wheels and leave the kit hopper empty. Open the LOLIN USB Serial
Monitor at **115200 baud**. Before the first hand gesture, use `c` if floor
colors need calibration. Otherwise, show a hand to the left APDS sensor,
hold the robot still while the gyro calibrates, then show a hand to the
right APDS sensor. The LOLIN starts camera UART after `START2` and prints
`NICLA: UART1 RX on GPIO12 active.`

The maze drive starts after arming, so keep the wheels clear of the table.
The `camera` Serial command is not available in this SNS sketch.

## 3. Present targets

Show one cognitive ring or Greek letter at a time. The LOLIN should print
`COGNITIVE` or `PSO` for a confirmed detection, then `RESCUE` with
the requested kit count. Harmed/Phi requests two kits, Stable/Psi one,
and Unharmed/Omega zero. The dropper servo is configured for **0° home**
and **90° release**; confirm those positions with an empty hopper first.
Remove the target and let both camera streams report clear before showing
another target.

If OpenMV's TX count rises but the LOLIN shows no detection, check that
ESLOV SDA_EXT is on GPIO12, the grounds are common, and the robot reached
`START2`. If detections are wrong, compare the OpenMV preview with the
actual target under the lighting used on the course. The simulated tests
in `NiclaVision/tests/test_simulated_targets.py` cover glyph shapes and
packet formatting; they do not verify physical camera recognition.
