# Maze Solver BLE serial

Open [MAZESOLVER_BLE.ino](MAZESOLVER_BLE.ino) in Arduino IDE and select
**ESP32 Dev Module**. Use the ESP32 board package's bundled BLE library and
the usual **Adafruit NeoPixel**, **MPU6050_light**, **Adafruit AS7341**, and
**Adafruit BusIO** libraries. Use the same `resetAngleZ()` helper in
`MPU6050_light.h` as the regular maze sketch.

## Connect from your phone

1. Upload by USB. In a BLE terminal that supports the Nordic UART Service,
   scan from inside the app and connect to **MazeSolver-BLE**.
2. Enable newline on send. If you need floor color calibration, send `c`
   first, then send `exit` when you finish.
3. Keep the robot still and send `start`. The sketch calibrates the gyro
   before enabling the motors.

The hand-gesture sequence also works. Show a hand to the left APDS, remove
it, and let the gyro calibrate. Then show a hand to the right APDS and remove
it, or send `start` from the terminal.

The phone receives serial messages printed after it connects. USB Serial
Monitor also works at **115200 baud**. Send `stop` with a newline from
either console to disable motor drive. Reset the ESP32 to start a new run.

The [ESP32 config](config/config_esp32_dev.h) contains the settings for the
complete maze routine.

| Setting | What it controls |
| --- | --- |
| `APDS_PROXIMITY_GAIN` | APDS proximity sensitivity (1×, 2×, 4×, or 8×); currently 4× in the main sketch |
| `leftWallOffset` | Filtered APDS target while following one wall |
| `wallSense` | Side sensor cutoff for straight steering |
| `kp_move`, `kd_move` | Straight steering gains |
| `MAX_WALL_CORRECTION` | Maximum PWM difference applied by wall and gyro steering |
| `FRONT_WALL_THRESHOLD` | Front APDS count that triggers front-wall handling when exceeded |
| `kp_turn`, `kd_turn` | Gyro turn gains |
| `OPENING_ADVANCE_MM` | Forward distance before turning into a side opening |
| `TURN_BACKUP_MM` | Backup distance after a single turn |

## Floor color calibration

Mount the AS7341 facing the floor and connect it to TCA CH2. The main
sketch turns on its illumination LED at 4 mA during setup.

1. Open Serial Monitor at **115200 baud** with **Newline** selected. Send
   `c` before the first hand gesture to enter calibration with the motors
   in standby.
2. Put a tile under the sensor at its mounted height. Send `read` to see
   the ten bands and the closest saved reference, when available.
3. Send `save red`, `save black`, or another lowercase name to store the
   average of five readings. Up to 16 references are saved in EEPROM and
   survive a reset. Saving the same name replaces it.
4. Use `list` to see the references, `delete red` to remove one, or
   `help` for the command list. Send `exit` to return to startup, then use
   `start` or the hand gestures.

Use the actual floor material at a consistent height for calibration and
comparison. The closest match is based on the raw band counts.

## Floor tiles while solving

Before starting, use `c` and save the real tiles under the mounted sensor
with `save black`, `save blue`, and `save silver`. Also save ordinary
floor with `save floor`. An existing `white` reference works as the
ordinary floor reference if it represents the course surface. References
already saved under those names are loaded when a run starts.

| Tile | Action |
| --- | --- |
| Black | Stop, pivot 180° using two existing left turns, and resume solving. |
| Blue | Stop for five seconds, flash the NeoPixels blue, and resume. |
| Silver | Flash the NeoPixels white while continuing to solve. |

Use `read` to check the `Driving match` result before starting. The robot
requires two consecutive confident readings before it reacts and waits for a
different confirmed surface before reacting to that color again.
Unknown readings and sensor read errors leave the current maneuver alone.
