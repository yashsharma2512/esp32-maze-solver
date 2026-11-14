# Maze settings

Open [MAZESOLVER.ino](MAZESOLVER.ino) in Arduino IDE and select
**ESP32 Dev Module**. See the [project README](../../README.md) for wiring,
libraries, and the hand-gesture start sequence.

The [ESP32 config](config/config_esp32_dev.h) contains the settings for the
complete maze routine.

| Setting | What it controls |
| --- | --- |
| `APDS_PROXIMITY_GAIN` | APDS proximity sensitivity (1×, 2×, 4×, or 8×); currently 2× |
| `leftWallOffset` | Filtered APDS target while following one wall |
| `wallSense` | Side sensor cutoff for straight steering |
| `kp_move`, `kd_move` | Straight steering gains |
| `MAX_WALL_CORRECTION` | Maximum PWM difference applied by wall and gyro steering |
| `FRONT_WALL_THRESHOLD` | Front APDS count that triggers front-wall handling when exceeded |
| `kp_turn`, `kd_turn` | Gyro turn gains |
| `OPENING_ADVANCE_MM` | Forward distance before turning into a side opening |
| `TURN_BACKUP_MM` | Backup distance after a single turn |

To stop a run, send `stop` at **115200 baud** with **Newline** selected.
Reset the ESP32 to run again.

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
   `help` for the command list. Send `exit` and use the hand gestures to
   start the maze routine.

Use the actual floor material at a consistent height for calibration and
comparison. The closest match is based on the raw band counts.
