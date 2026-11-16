# SNS maze

Open `MAZESOLVER.ino` in Arduino IDE with the entire `MAZESOLVER` folder intact.
Select **LOLIN S2 Mini** and use **Adafruit NeoPixel** and **MPU6050_light**.
The original sketch calls `mpu.resetAngleZ()`. If your library lacks it, add
`void resetAngleZ() { angleZ = 0; }` in the public section of its header.

## APDS9960 wiring

Connect the downward APDS9960 SDA to TCA9548A **SD7** and SCL to **SC7**.
Use power compatible with your breakout and a common ground. The original
I²C bus uses GPIO16 for SDA and GPIO17 for SCL. The original navigation APDS
channels remain CH3 (front), CH2 (left), and CH4 (right). The encoders remain on
CH5 (left) and CH6 (right).

Use steady visible light on the floor during calibration and driving. If the
sensor is shaded by the chassis, aim a steady white light at its view of the
floor. Keep the robot's colored status LEDs out of the sensor's view.

## Calibrate at the mounted height

1. Open Serial Monitor at **115200 baud** with **Newline** selected. Reset the
   robot and send `c` before the first hand gesture.
2. Put normal floor under the sensor and send `save floor`. Repeat with the
   black hazard tile and `save black`, then the blue puddle and `save blue`.
3. Send `read` on each surface. Check that `Driving match` shows `floor`,
   `black`, or `blue` on the corresponding tile. References persist in flash.
4. Send `exit`, then use the original left and right hand gestures to start.

| Tile | Action |
| --- | --- |
| Black hazard | Stop, make two of the original 90° left turns, and resume from a fresh wall check. |
| Blue puddle | Stop for five seconds, flash the NeoPixels blue, then resume. |

The sketch requires two consecutive matching samples. A confirmed different
surface re-arms the previous tile. Floor actions run during wall following
and forward corner advances. Without a floor and a tile reference, the
original maze run starts with floor detection disabled.
