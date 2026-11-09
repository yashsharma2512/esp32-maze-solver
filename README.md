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

## Libraries

Install **Adafruit NeoPixel** and **MPU6050_light 1.2.1**. The original
sketch uses this helper in the public section of the library's
`MPU6050_light.h`. Add it if your installed copy is missing it.

    void resetAngleZ() { angleZ = 0; }

## Wiring

| Device | Connection |
| --- | --- |
| ESP32 I²C | GPIO21 SDA, GPIO22 SCL, common ground |
| TCA9548A | Main bus at `0x70` |
| MPU6050 | Main bus at `0x68` |
| Left, right, front APDS-9960 | TCA CH0, CH1, CH6 respectively |
| Left, right AS5600 | TCA CH5, CH4 respectively |
| TB6612 left IN1 / IN2 / PWM | GPIO25 / GPIO26 / GPIO27 |
| TB6612 right IN1 / IN2 / PWM | GPIO18 / GPIO19 / GPIO23 |
| TB6612 STBY | GPIO33 |
| Left, right click switches | GPIO32, GPIO13. Connect NO to the GPIO and COM to ground. |
| Optional NeoPixel data | GPIO4 |
| Optional victim-detection UART | GPIO16 RX, GPIO17 TX |

## Start a run

Open Serial Monitor at **115200 baud**. Lift the wheels for the first test.
Show a hand to the left APDS and remove it. Keep the robot still until
`MPU calibrated.` appears. Then show a hand to the right APDS and remove it
to start. Use the motor power switch to stop the run.
