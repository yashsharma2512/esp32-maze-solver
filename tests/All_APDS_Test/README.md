# All APDS-9960 sensor test

Upload [All_APDS_Test.ino](All_APDS_Test.ino) to the **ESP32 Dev Module**.
The sketch keeps TB6612 `STBY` on GPIO33 low, so it cannot drive the motors.
Open Serial Monitor at **115200 baud**. Every 200 ms it shows raw and filtered
proximity values for all three APDS sensors:

| Sensor | TCA9548A channel |
| --- | ---: |
| Left | CH0 |
| Right | CH1 |
| Front | CH6 |

The ESP32 I2C pins are GPIO21 SDA and GPIO22 SCL. The TCA address is `0x70`;
each APDS uses `0x39` behind its own mux channel. Gain is **2×**, and the
filter coefficient is **0.65**, matching the USB firmware and BLE sketch. The Wi-Fi and tuning sketches currently use 1×. Change `APDS_GAIN` in this test to 1 before comparing against those sketches. The
sketch uses the same APDS register settings as `proximity.ino`.

At startup, each sensor should print an APDS ID and `ready`. Move a hand or
light card in front of one sensor at a time. Its `raw` value should change
while the other two remain relatively steady. `READ_ERROR` or `INIT_ERROR`
means that sensor or its mux channel did not respond. `SATURATED` means the
raw value reached 255. A genuine reading of zero appears as `raw=  0`.

This is a separate sketch: uploading it temporarily replaces the maze
firmware on the ESP32. Re-upload the robot sketch after testing.
