# SNS maze

Open `MAZESOLVER.ino` in Arduino IDE with the entire `MAZESOLVER` folder intact.
Select **LOLIN S2 Mini** and use **Adafruit NeoPixel** and **MPU6050_light**.
The original sketch calls `mpu.resetAngleZ()`. If your library lacks it, add
`void resetAngleZ() { angleZ = 0; }` in the public section of its header.
Keep `src/NiclaVision` beside the sketch; it contains the Nicla receiver,
target LED, and rescue dropper code.

## Nicla Vision and dropper

The LOLIN pin assignments are in `config/config_lolins2.h`. Connect Nicla
Vision **TX** to LOLIN **GPIO12** (`NICLA_RX_PIN`) and connect their grounds.
Both boards use **115200 baud**. GPIO14 (`NICLA_TX_PIN`) is reserved for a
future return link; the current Nicla protocol only needs its TX wire.
Connect the rescue kit servo signal to LOLIN **GPIO2** (`DROPPER_PIN`), and
power the servo from a suitable separate supply with a common ground.

The receiver accepts the WiFi sketch's `type,stable_ms,x,y` packets for
cognitive rings (0–2) and Φ/Ψ/Ω letters (3–5). It requires three matching
fresh packets, reports detections over USB Serial, and shows target colors
on the two NeoPixels. A confirmed rescue stops the motors and dispenses
two kits for harmed/Phi, one for stable/Psi, and none for unharmed/Omega.
It waits for a confirmed clear scene before handling another encounter.
Set `DROPPER_HOME_DEG` and `DROPPER_RELEASE_DEG` in
`src/NiclaVision/DropperConfig.h` after checking the actual mechanism.
At the startup prompt or in color calibration, send `camera` with a newline
to check UART packets and current target status.

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
