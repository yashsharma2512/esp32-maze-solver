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

To calibrate wheel travel independently of the maze behavior, use the
[AS5600 distance move test](tests/Encoder_Distance_Test/README.md). It moves a
set distance using both encoders and shows their counts and calculated travel.

## Libraries

Install **Adafruit NeoPixel** and **MPU6050_light 1.2.1**. The original
sketch uses this helper in the public section of the library's
`MPU6050_light.h`. Add it if your installed copy is missing it.

    void resetAngleZ() { angleZ = 0; }

The main sketch and tuning stage 4 also use **Adafruit AS7341** and **Adafruit BusIO**.

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
to start. Use the motor power switch to stop the run.

For floor color readings or calibration, send `c` before the first hand
gesture. [AS7341 calibration commands](firmware/MAZESOLVER/README.md#floor-color-calibration)
