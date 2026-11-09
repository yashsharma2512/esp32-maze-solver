# Maze settings

Open [MAZESOLVER.ino](MAZESOLVER.ino) in Arduino IDE and select
**ESP32 Dev Module**. See the [project README](../../README.md) for wiring,
libraries, and the hand-gesture start sequence.

The [ESP32 config](config/config_esp32_dev.h) contains the settings for the
complete maze routine.

| Setting | What it controls |
| --- | --- |
| `leftWallOffset` | Filtered APDS target while following one wall |
| `wallSense` | Side sensor cutoff for straight steering |
| `kp_move`, `kd_move` | Straight steering gains |
| `FRONT_WALL_THRESHOLD` | Front APDS count that triggers front-wall handling when exceeded |
| `kp_turn`, `kd_turn` | Gyro turn gains |
| `OPENING_ADVANCE_MM` | Forward distance before turning into a side opening |
| `TURN_BACKUP_MM` | Backup distance after a single turn |
