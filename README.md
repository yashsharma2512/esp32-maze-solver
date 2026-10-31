# ESP32 Maze Solver Hardware Tests

Standalone Arduino hardware tests for a maze robot using a classic 38-pin
ESP32-WROOM-32 development board. This repository currently contains hardware
tests, not the complete maze-solving firmware.

## Sketches

| Sketch | Purpose |
| --- | --- |
| `tests/TCA_MPU_Test/TCA_MPU_Test.ino` | Scan I2C, verify TCA9548A channel control, identify an MPU6050 or MPU6500, and print acceleration and angular velocity. |
| `tests/Motor_Test/Motor_Test.ino` | Test each motor forward/reverse, both motors together, and left/right pivots. Uses the maze sketch's `setMotorIOs()` and `moveMotor(left, right)` interface. |
| `tests/AS5600_TCA9548A_Test/AS5600_TCA9548A_Test.ino` | Scan TCA channels and read both AS5600 wheel encoders, with startup tare and magnet diagnostics. |

Open each sketch separately in Arduino IDE. Select **ESP32 Dev Module**, choose
the board's port, upload, and open Serial Monitor at **115200 baud**. The sketches
use the ESP32 Arduino board package's built-in libraries. All three sketches
compile for **ESP32 Dev Module**; verify behavior on the assembled robot.

## I2C wiring

For the default test, connect the IMU directly to the main bus alongside the TCA.

| ESP32 | TCA9548A | IMU |
| --- | --- | --- |
| GPIO21 | SDA | SDA |
| GPIO22 | SCL | SCL |
| 3V3 | VIN/VCC | VCC, on a 3.3 V supply-compatible breakout |
| GND | GND | GND |

Connect TCA A0/A1/A2 to GND for address `0x70`; keep RESET high. Connect IMU AD0
to GND for address `0x68`. The IMU INT pin is unused. Keep ESP32-side I2C pull-ups
at 3.3 V and verify the supply requirements of the actual breakout modules.

`MPU_CHANNEL = -1` means main-bus IMU. If the IMU is connected through the TCA,
set this constant to the channel number, 0 through 7. Connect that channel's
SDx to SDA and SCx to SCL.

Main-bus devices remain visible during every channel scan. Seeing `0x68` on all
eight scans does not mean that eight sensors are present. A TCA control test PASS
checks register write/read behavior; external sensors are needed to verify each
physical downstream connection.

### Identity observed during troubleshooting

The reported hardware output showed the sensor at bus address `0x68` returning
`WHO_AM_I = 0x70`. This matches the MPU6500 identity, while MPU6050 normally returns
`0x68`. The updated test supports both identities and repeats the identity read
before configuring the device. Identity alone does not establish full sensor
health: move/rotate the board and inspect the changing readings.

## AS5600 encoder wiring and test

Both AS5600 sensors use I2C address `0x36`, so connect them to separate TCA9548A
channels. Use the main I2C wiring above (`GPIO21` SDA, `GPIO22` SCL), with the
**left encoder on TCA channel 5** and **right encoder on channel 4**. The encoder
test does not enable the motors. Turn each wheel by hand while watching Serial
Monitor at 115200 baud.

At startup the test scans all eight mux channels, then tares each encoder so its
`tared_angle` and `total` begin near zero. `raw` remains the sensor's absolute
0–4095 reading. Send `z` to tare both again. The output also shows per-sample
`delta`, RPM, status, AGC, magnetic magnitude, and a magnet strength label. If
`magnet=TOO WEAK` persists, check magnet type, centering, and air gap before
using the readings for distance control. The revised magnetic diagnostics are
compile-checked but have not yet been validated from a live serial log.

## Motor wiring

**Assumes a TB6612FNG driver. Confirm the actual driver model before wiring.**

| ESP32 GPIO | Driver pin | Sketch name |
| --- | --- | --- |
| 25 | AIN1 | LMF |
| 26 | AIN2 | LMR |
| 27 | PWMA | PWML |
| 18 | BIN1 | RMF |
| 19 | BIN2 | RMR |
| 23 | PWMB | PWMR |
| 33 | STBY | STBY |
| 3V3 | VCC | Logic supply |
| GND | GND | Common ground |

Connect AO1/AO2 to the left motor and BO1/BO2 to the right motor. Connect VM to a
motor supply compatible with the motors and driver; join its negative to common
GND. Do not power the motors from the ESP32 3V3 pin.

**Lift the wheels before uploading.** The test starts automatically after three
seconds, runs each movement for 1.5 seconds with stops between movements, then
disables the driver. Press EN/Reset to repeat. Default speed is 120 out of 255;
adjust `TEST_SPEED` if needed. It retains the original sketch's 0.985 left trim.
If a motor's forward direction is reversed, power off before swapping that
motor's output wires.

Positive `moveMotor(left, right)` values mean forward, negative values mean
reverse, and zero means stop. This is an open-loop observation test; there is no
encoder feedback or automatic confirmation that the wheels moved correctly.

## References

- [ESP32-DevKitC pin reference](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html)
- [TCA9548A datasheet](https://www.ti.com/lit/ds/symlink/tca9548a.pdf)
- [MPU6500 register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf)
- [AS5600 datasheet](https://www.mouser.com/datasheet/2/588/AMS_AS5600_Datasheet_EN_v1-372293.pdf)
- [TB6612FNG hookup guide](https://learn.sparkfun.com/tutorials/tb6612fng-hookup-guide/all?print=1)
