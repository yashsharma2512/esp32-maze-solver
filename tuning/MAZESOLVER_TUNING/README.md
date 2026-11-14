# Maze flow tuning

Open [MAZESOLVER_TUNING.ino](MAZESOLVER_TUNING.ino) and select
**ESP32 Dev Module**. Set `FLOW_TEST_STAGE` in
[config/config_esp32_dev.h](config/config_esp32_dev.h), then upload. The front
APDS is on CH6.

Set `APDS_PROXIMITY_GAIN` in that config to 1, 2, 4, or 8. It applies to the
left, right, and front APDS sensors. All four sketches currently use 2×.
Higher gain raises proximity counts for a given
target but can saturate at 255. Recheck wall thresholds after changing it.

The shared wall-follow baseline is `leftWallOffset = 180`, `kp_move = 0.50`,
`kd_move = 0.25`, and `MAX_WALL_CORRECTION = 40`. The correction cap applies
after wall and gyro steering are added, so straight driving stays between
PWM 60 and 140 at the current base speed of 100. To refine the wall gap,
run stage 1 beside a straight left wall and set `leftWallOffset` to the
filtered `L` reading at the desired distance. If it still weaves, reduce
`kp_move` before increasing `kd_move`. Keep the same values in the main,
Wi-Fi, and BLE configs when moving from tuning to a full run.

Open Serial Monitor at **115200 baud**. Lift the wheels for the first startup
check. Show your hand to the left APDS, remove it, and let the gyro calibrate
while the robot is still. Show your hand to the right APDS to start.
Send `stop` with **Newline** selected to pause motor drive. Send `start` to
resume with a fresh wall check.
Send `reset` to reboot and return to the start of the selected stage.

Stage 4 also initializes the bottom-facing AS7341 on TCA CH2 and enables its
illumination LED at 4 mA. Before the first hand gesture, send `c` to calibrate
floor colors with the motors stopped. Set Serial Monitor to **Newline**; use
`save red`, `list`, `read`, `delete red`, and `exit`. Stages 1–3 do not initialize
the color sensor. See the [color calibration guide](../../firmware/MAZESOLVER/README.md#floor-color-calibration).

| Stage | Course setup | What to tune |
| --- | --- | --- |
| 1 | Straight wall on the left, open right side, then a wall ahead. The robot stops at the front wall. | Use filtered L at the desired gap for leftWallOffset. Compare readings with and without a wall before changing wallSense. Check P and D before tuning kp_move or kd_move. Use F to tune FRONT_WALL_THRESHOLD. |
| 2 | Block the front with one side open. Repeat with the other side open, then try a dead end. | Compare the measured turn angle against 90 degrees before changing kp_turn or kd_turn. Check TURN_BACKUP_MM on the floor. |
| 3 | Add a single left opening with clear space ahead. Mark where the pivot starts and ends. | Check OPENING_ADVANCE_MM and TURN_BACKUP_MM. Check encoder distance scaling first if both moves measure wrong. |
| 4 | Run the complete route with the original click-switch and victim-detection paths enabled. The AS7341 illumination LED is on. | Verify how the features interact using the settings from the earlier stages. |

Tune one value per run. In stages 1 through 3 an APDS read failure stops the
test and reports the channel. Fix a repeated failure before driving on the floor.
