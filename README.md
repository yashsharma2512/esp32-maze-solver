# ESP32 Maze Solver

Open [MAZESOLVER.ino](firmware/MAZESOLVER/MAZESOLVER.ino) in Arduino IDE and
select **ESP32 Dev Module** for the 38-pin ESP32. Keep the whole sketch
folder together so Arduino IDE can compile all the tabs.

The firmware runs the full left-wall maze routine, including front-wall turns,
side openings, dead ends, and click-switch recovery. See the
[maze settings guide](firmware/MAZESOLVER/README.md) for steering and turn
parameters.

For stage-by-stage testing, open the separate
[MAZESOLVER_TUNING.ino sketch](tuning/MAZESOLVER_TUNING/MAZESOLVER_TUNING.ino).
Choose stages 1–4 in its own config file. [Flow-tuning guide](tuning/MAZESOLVER_TUNING/README.md)

For BLE serial and the `start` command, open the separate BLE sketch.
[BLE setup and commands](ble/MAZESOLVER_BLE/README.md)

The Wi-Fi sketch hosts a browser console in AP+STA mode and accepts OTA
uploads during startup. [Wi-Fi setup and OTA](wifi/MAZESOLVER_WIFI/README.md)
For the victim camera, use the [Nicla Vision setup](vision/NiclaVision/README.md).

To calibrate wheel travel independently of the maze behavior, use the
[AS5600 distance move test](tests/Encoder_Distance_Test/README.md). It moves a
set distance using both encoders and shows their counts and calculated travel.

## Libraries

Install **Adafruit NeoPixel** and **MPU6050_light 1.2.1**. The original
sketch uses this helper in the public section of the library's
`MPU6050_light.h`. Add it if your installed copy is missing it.

    void resetAngleZ() { angleZ = 0; }

The main, Wi-Fi, and BLE sketches and tuning stage 4 also use **Adafruit AS7341**
and **Adafruit BusIO**.

## Wiring

| Device | Connection |
| --- | --- |
| ESP32 I²C | GPIO21 SDA, GPIO22 SCL, common ground |
| TCA9548A | Main bus at `0x70` |
| MPU6050 | Main bus at `0x68` |
| Left, right, front APDS-9960 | TCA CH0, CH1, CH6 respectively |
| Bottom-facing AS7341 | TCA CH2. Connect SDA to SD2, SCL to SC2, and ground to common GND. |
| Left, right AS5600 | TCA CH5, CH4 respectively |
| TB6612 left IN1 / IN2 / PWM | GPIO25 / GPIO26 / GPIO27 |
| TB6612 right IN1 / IN2 / PWM | GPIO18 / GPIO19 / GPIO23 |
| TB6612 STBY | GPIO33 |
| Left, right click switches | GPIO32, GPIO13. Connect NO to the GPIO and COM to ground. |
| Optional NeoPixel data | GPIO4 |
| Optional victim-detection UART | GPIO16 RX, GPIO17 TX |

For the Adafruit AS7341 breakout, connect VIN to 3.3 V.
[AS7341 pinout](https://learn.adafruit.com/adafruit-as7341-10-channel-light-color-sensor-breakout/pinouts)

## Start a run

Open Serial Monitor at **115200 baud**. Lift the wheels for the first test.
Show a hand to the left APDS and remove it. Keep the robot still until
`MPU calibrated.` appears. Then show a hand to the right APDS and remove it
to start. Send `stop` over USB Serial with **Newline** selected to disable
motor drive. Reset the ESP32 to start a new run. The BLE and Wi-Fi sketches
also accept `stop` over their wireless consoles.

## Floor color calibration

The bottom-facing AS7341 is on TCA CH2. Its illumination LED turns on at
4 mA during setup. Calibrate with the sensor at its normal height above the
actual course tiles and the robot stopped. Open USB Serial Monitor at
**115200 baud** with **Newline** selected. The [Wi-Fi sketch](wifi/MAZESOLVER_WIFI/README.md)
also accepts these commands in its browser console; the [BLE sketch](ble/MAZESOLVER_BLE/README.md)
accepts them over BLE.

1. Before the first hand gesture or `start`, send `c` to enter calibration.
2. Put ordinary floor under the sensor and send `save floor`. Put each tile
   under the sensor in turn and send `save black`, `save blue`, and
   `save silver` for the tile types you have. An existing `white` reference
   can serve as ordinary floor if it represents the course surface.
3. Send `read` on each surface. It shows the ten sensor bands, the closest
   saved reference, and, in the Wi-Fi and BLE sketches, the `Driving match`.
   Send `list` to see stored references; `delete black` removes one.
4. Send `exit`, then use `start` or the hand gestures supported by your
   sketch to begin the run.

Each `save` averages five readings of F1–F8, Clear, and NIR and stores the
reference in flash. Up to 16 named references survive a reset; saving the
same name replaces it. During a run, the Wi-Fi and BLE sketches compare new
readings against the saved references and reject uncertain matches. Floor
detection needs a `floor` or `white` reference and at least one of `black`,
`blue`, or `silver`. It confirms a tile with two readings before reacting:
black causes a U-turn, blue stops the robot for five seconds, and silver
flashes the NeoPixels.

The [main firmware](firmware/MAZESOLVER/README.md#floor-color-calibration) and
tuning stage 4 include the calibration commands but do not act on floor
colors during maze runs. Upload the Wi-Fi or BLE sketch to use the tile
actions.
