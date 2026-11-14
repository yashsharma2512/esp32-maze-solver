# Maze Solver Wi-Fi

Open [MAZESOLVER_WIFI.ino](MAZESOLVER_WIFI.ino) in Arduino IDE and select
**ESP32 Dev Module**. Install **Adafruit NeoPixel**, **MPU6050_light**,
**Adafruit AS7341**, and **Adafruit BusIO**. Use the same `resetAngleZ()`
helper in `MPU6050_light.h` as the regular maze sketch.

## Connect to the browser console

1. Upload over USB. The **MazeSolver** hotspot, browser console, and OTA
   upload password is **`12345678`**. To also join an existing router, copy
   `WifiSecrets.example.h` to `WifiSecrets.h` and fill in that router's SSID
   and password. The router password must match the router's own settings.
2. Open USB Serial Monitor at **115200 baud** to see the connection details.
   Keep **Erase All Flash Before Sketch Upload** disabled to retain saved
   color references.
3. Join the **MazeSolver** hotspot and open **http://192.168.4.1** in a
   browser. On the router's network, use the **Router console** IP printed at
   startup. Sign in as `maze` with password `12345678`.
4. Use the page to read logs and send commands. For color calibration, send
   `c`, then `exit` when finished. Keep the robot still and send `start`.
   The sketch calibrates the gyro before enabling the motors.

The hand-gesture sequence also works. Show a hand to the left APDS, remove
it, and let the gyro calibrate. Then show a hand to the right APDS and remove
it, or send `start` from the terminal.

USB Serial Monitor accepts the same commands at **115200 baud** with
**Newline** selected. During a run, type `stop` in either console or press
**Stop motors** in the browser. This disables motor drive through the
TB6612 **STBY** pin on **GPIO33** and pauses the run. The wheels can coast.
Send `start` to resume with a fresh wall check. If the console is unreachable, use the
physical motor power switch.

Send `reset` from either console to reboot the ESP32. The browser console
disconnects briefly. Rejoin the hotspot if needed, then use the normal
startup sequence. Saved floor-color references stay in EEPROM.

Send `camera` from either console before `start` to check the Nicla UART
and the current victim classification. [Nicla Vision wiring and setup](https://github.com/yashsharma2512/esp32-maze-solver/tree/main/vision/NiclaVision)

## Upload updates over Wi-Fi

Reset the robot and leave it at the startup prompt with the motors in
standby. Connect your PC to the same router or to the MazeSolver hotspot.
Choose the **mazesolver** network port under **Tools > Port** in Arduino
IDE, upload, and enter `12345678` when prompted. The
robot restarts after a successful upload. OTA closes when a run begins.

The [ESP32 config](config/config_esp32_dev.h) contains the settings for the
complete maze routine.

| Setting | What it controls |
| --- | --- |
| `APDS_PROXIMITY_GAIN` | APDS proximity sensitivity (1×, 2×, 4×, or 8×) |
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
comparison. Each saved reference is the average of five readings across
F1–F8, Clear, and NIR. Matching compares all ten raw band counts and rejects
readings that are too far from a reference. An action tile match is also
rejected if another saved color is nearly as close. Saving an existing name
replaces its previous reference.

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
