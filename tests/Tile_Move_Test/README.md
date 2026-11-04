# One tile movement test

Standalone ESP32-WROOM-32 sketch for a single encoder measured move. It uses
the original firmware's signed motor commands, left motor trim, encoder tare
before each move, and distance based speed range. It uses the robot's actual
TCA channels: left AS5600 on 5 and right AS5600 on 4. It does not load the
left wall follow sketch or the maze solver.

## Run

1. Open `Tile_Move_Test.ino` in Arduino IDE, select **ESP32 Dev Module**, and
   upload. Open Serial Monitor at **115200 baud**.
2. Check that both idle magnet labels say `OK`. The sketch blocks movement if
   an encoder cannot be read or its magnet status is unhealthy.
3. For the first run, lift the wheels and send `g` to check that both spin
   forward and both measured distances increase. Send `x` to stop.
4. Put the robot at the center of a tile with at least 30 cm of clear floor
   ahead. Send `g` or briefly press BOOT **after startup** to move forward one
   tile. Each new command tares both encoders. Send `b` for one tile backward.
   BOOT or `x` stops an active move.
5. Measure center to center travel and adjust `TILE_LENGTH_MM` or
   `WHEEL_DIAMETER_MM` near the top of the sketch if needed. Current values
   are **300 mm** and **70 mm**, as measured for this robot.

The driver stays in standby while idle. A move stops at the measured tile
length, on an encoder/magnet fault, a stall, excessive wheel mismatch, or a
15 second fault timeout. The timeout prevents a motor from running forever
when the distance target cannot be reached; it is not a fixed run duration.
There is no front obstacle sensor in this sketch, so use a clear path. Do not
hold BOOT while resetting or powering the ESP32 because GPIO0 selects its boot
mode.

## Wiring used

| Part | ESP32 / TCA9548A |
| --- | --- |
| TCA SDA, SCL | GPIO21, GPIO22 |
| Left AS5600 | TCA channel 5, address 0x36 |
| Right AS5600 | TCA channel 4, address 0x36 |
| Left TB6612 AIN1, AIN2, PWMA | GPIO25, GPIO26, GPIO27 |
| Right TB6612 BIN1, BIN2, PWMB | GPIO18, GPIO19, GPIO23 |
| TB6612 STBY | GPIO33 |
| DevKit BOOT | GPIO0 |

The original `MoveStrightToDistance()` also reads the MPU and front sensors.
This standalone test uses encoder distance balance instead so movement can be
checked before those sensors are integrated. The original `dfs()` is still an
empty TODO; this sketch is the tile movement building block, not a DFS maze
search.

Compiled with `arduino-cli compile --fqbn esp32:esp32:esp32`.
