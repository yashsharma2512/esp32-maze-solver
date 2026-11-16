# Code guide

This repository contains several independent robot sketches. Choose the sketch
for your board and task, then keep its complete folder together when opening it
in Arduino IDE. Uploading a sketch replaces the program currently on that board.

| Folder | Board and entry point | Purpose |
| --- | --- | --- |
| [firmware/MAZESOLVER](firmware/MAZESOLVER/README.md) | ESP32 Dev Module, `MAZESOLVER.ino` | Full USB-controlled maze run and floor-color calibration. |
| [wifi/MAZESOLVER_WIFI](wifi/MAZESOLVER_WIFI/README.md) | ESP32 Dev Module, `MAZESOLVER_WIFI.ino` | Full maze run with browser console, startup OTA and floor actions. |
| [ble/MAZESOLVER_BLE](ble/MAZESOLVER_BLE/README.md) | ESP32 Dev Module, `MAZESOLVER_BLE.ino` | Full maze run with BLE console and floor actions. |
| [tuning/MAZESOLVER_TUNING](tuning/MAZESOLVER_TUNING/README.md) | ESP32 Dev Module, `MAZESOLVER_TUNING.ino` | Four selectable stages for isolating movement. |
| [SNS maze/MAZESOLVER](SNS%20maze/MAZESOLVER/README.md) | LOLIN S2 Mini, `MAZESOLVER.ino` | SNS robot with different pins, APDS floor sensor and Nicla link. |

The ESP32 Wi-Fi and tuning sketches currently use APDS proximity gain 1×.
The USB firmware and BLE sketch use 2×. Each sketch has its own config and
its own `src/NiclaVision` headers. Editing one copy does not update the others.

## ESP32 Wi-Fi run

1. Open [MAZESOLVER_WIFI.ino](wifi/MAZESOLVER_WIFI/MAZESOLVER_WIFI.ino),
   select **ESP32 Dev Module**, and install the libraries listed in its guide.
2. Check GPIO and TCA channel assignments in
   [config_esp32_dev.h](wifi/MAZESOLVER_WIFI/config/config_esp32_dev.h).
   The project README lists the ESP32 wiring. Connect common ground.
3. Open Serial Monitor at 115200 baud with Newline selected. For a first
   check, raise the wheels. Send `c` at startup to calibrate floor colors,
   then `exit`. Send `start` and keep the robot still for gyro calibration.
4. Send `stop` to pause, `start` to resume, and `reset` to reboot. The
   Wi-Fi guide explains the browser console, credentials, and startup OTA.
5. Before driving on a course, check each sensor and motor and measure the
   threshold values described below.

## Navigation and tuning

In the Wi-Fi sketch, `proximity.ino` reads front, left and right APDS sensors
and smooths the side readings. Higher counts generally indicate a closer
reflective object. A value of 255 is the maximum output, so it cannot show how
much closer a wall is once the reading has reached that value.

`maze.ino` decides the route. When the front threshold is exceeded, it checks
the side readings. Left and front closed with right open selects a right turn.
Both sides closed selects a U-turn. `motors.ino` contains straight steering,
turning and distance moves. `mpu.ino` and the MPU6050 library provide gyro
measurements. `encoder.ino` reads the wheel encoders. `FloorTiles.ino`
handles floor actions in the Wi-Fi and BLE versions.

| Wi-Fi setting | Current value | Meaning |
| --- | ---: | --- |
| `APDS_PROXIMITY_GAIN` | 1× | Sensor gain. Changing it changes the readings used for driving and startup. |
| `leftTurnSense`, `rightTurnSense` | 9, 9 | A filtered side reading above its threshold counts as a wall for turns. |
| `FRONT_WALL_THRESHOLD` | 200 | Front reading above this starts front-wall handling. |
| `leftWallOffset`, `rightWallOffset` | 180, 50 | Target proximity for straight wall steering. |
| `wallSense` | 25 | Side-wall cutoff for straight steering. |
| `kp_move`, `kd_move` | 0.50, 0.25 | Straight steering proportional and derivative gains. |
| `OPENING_ADVANCE_MM`, `TURN_BACKUP_MM` | 200, 30 | Corner advance and backup distances. |

For a wrong U-turn, record the right filtered reading with the right passage
open and again with a right wall at the intended detection distance. Pick a
`rightTurnSense` between readings only if the two ranges are clearly
separated. The APDS test defaults to 2×, so set its `APDS_GAIN` to 1 before
comparing with the current Wi-Fi configuration. Recheck both sides and the
front after changing sensor sensitivity. `LEFTWALLOFFSET` and
`RIGHTWALLOFFSET` are separate alignment constants. The lowercase
`leftWallOffset` and `rightWallOffset` control straight wall following.

The [tuning guide](tuning/MAZESOLVER_TUNING/README.md) describes the stage
order. Its values are independent of the Wi-Fi sketch. Transfer a measured
setting deliberately and test it again after uploading.

## Camera and rescue dropper

The ESP32 [Nicla guide](vision/NiclaVision/README.md) covers camera-only
`main.py` and `robot_uart.py`. The latter sends both detector results to the
ESP32. Its D1/TX goes to ESP32 GPIO35 with common ground. The
[SNS Nicla guide](SNS%20maze/NiclaVision/README.md) describes the separate
SNS camera program and ESLOV-to-GPIO12 wiring.

After three matching fresh camera packets, the ESP32 rescue routine stops
the robot during forward travel, sets the target color and requests two kits
for harmed/Phi, one for stable/Psi, or zero for unharmed/Omega. A clear
scene re-arms the next encounter. The Wi-Fi sketch uses GPIO2 and its own
`src/NiclaVision/DropperConfig.h` currently specifies 0° home and 90°
release. Confirm the mechanism and one-kit cycle with the hopper empty
before checking actual dispensing. Use the camera status and rescue log
messages in the Nicla guide to distinguish camera reception from servo motion.

## Individual tests

| Test | Use and expected output |
| --- | --- |
| [All APDS](tests/All_APDS_Test/README.md) | Prints left CH0, right CH1 and front CH6 raw and filtered readings. It holds motor standby low. |
| [Encoder distance](tests/Encoder_Distance_Test/README.md) | Commands a measured forward or reverse move and reports each encoder. It drives motors. |
| [Dropper calibration](tests/Dropper_Calibration/README.md) | Commands a servo angle from USB Serial while motor standby is low. |
| [Grove Vision AI V2](tests/Grove_Vision_AI_V2_Test/README.md) | Tests a separately trained Grove model. |
| [SNS simulated targets](SNS%20maze/NiclaVision/tests/test_simulated_targets.py) | Runs camera classification and packet checks on a computer. It does not exercise physical hardware. |

A successful compile or a simulated test does not establish the physical
stopping distance, camera recognition, motor direction or kit delivery.
Use the console output and staged tests to verify those on the actual robot.
