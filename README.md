# ESP32 Maze Solver

Arduino firmware and standalone hardware tests for a maze robot using a classic
38-pin ESP32-WROOM-32 development board.

The [main firmware](firmware/MAZESOLVER/README.md) runs continuous left-hand
wall following with the current three-APDS wiring and gyro turns. Open
`firmware/MAZESOLVER/MAZESOLVER.ino` in Arduino IDE.

## Sketches

| Sketch | Purpose |
| --- | --- |
| `tests/TCA_MPU_Test/TCA_MPU_Test.ino` | Scan I2C, verify TCA9548A channel control, identify an MPU6050 or MPU6500, and print acceleration and angular velocity. |
| `tests/Motor_Test/Motor_Test.ino` | Test each motor forward/reverse, both motors together, and left/right pivots. Uses the maze sketch's `setMotorIOs()` and `moveMotor(left, right)` interface. |
| `tests/AS5600_TCA9548A_Test/AS5600_TCA9548A_Test.ino` | Scan TCA channels and read both AS5600 wheel encoders, with startup tare and magnet diagnostics. |
| `tests/APDS9960_TCA_CH0_Test/APDS9960_TCA_CH0_Test.ino` | Initialize one APDS-9960 through TCA channel 0 and print raw and smoothed proximity values. |
| `tests/APDS9960_All_Test/APDS9960_All_Test.ino` | Check the left, right, and front APDS sensors and compare readings with maze firmware thresholds. |
| `tests/AS7341_Color_Test/AS7341_Color_Test.ino` | Read eight visible spectral bands, Clear, and Near IR from an AS7341 on TCA channel 2. |
| `tests/Unified_Robot_Test/Unified_Robot_Test.ino` | Test documented motors, IMU, APDS sensors, and encoders together, with USB Serial and a local Wi-Fi dashboard. |
| `tests/Left_Wall_Follow_Test/Left_Wall_Follow_Test.ino` | Run continuous straight left-wall tracking with APDS on TCA channel 0 and the documented motor pins. |
| `tests/Tile_Move_Test/Tile_Move_Test.ino` | Move forward or backward one encoder-measured 300 mm tile per command. |

Open each sketch separately in Arduino IDE. Select **ESP32 Dev Module**, choose
the board's port, upload, and open Serial Monitor at **115200 baud**. The AS7341
test requires the two libraries listed below. The other sketches use the ESP32
Arduino board package's built-in libraries. Verify behavior on the assembled robot.

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

## AS7341 color sensor test

On a 3.3 V-compatible breakout, connect VCC/VIN to ESP32 3V3 and GND to common
ground. Connect AS7341 SDA to **TCA SD2** and SCL to **TCA SC2**. The TCA input
bus stays on ESP32 GPIO21 (SDA) and GPIO22 (SCL). Install **Adafruit AS7341** and
**Adafruit BusIO** in Arduino Library Manager. Upload `AS7341_Color_Test.ino` and
open Serial Monitor at 115200 baud. Look for `TCA channel 2 selected.` at startup.

The sketch checks the sensor ID at `0x39`, turns on the breakout's illumination
LED at 4 mA, and prints ten raw readings per line. The LED stays on during
sampling. For a color comparison, hold different surfaces at the same distance
from the sensor under steady ambient light. The output shows the spectral bands,
Clear, and Near IR.

To calibrate for the robot's floor, keep the sensor at its mounted height with
the LED on. Place a matte black reference underneath and send `b`, then place a
matte white reference at the same distance and send `w`. Each reference averages
12 scans. After both captures, `REL_PCT` prints values in the same order as the
raw readings. Black is 0% and white is 100% for each band. Readings outside that
reference range can go below 0% or above 100%. `NA` means the band clipped or
the references differ by fewer than 20 counts. The values compare surfaces in
this setup. They are not absolute reflectance measurements.

Record the reference lines and several `REL_PCT` lines from each actual floor
material. References are held in RAM until reset. Send `r` to clear them. If you
change the sensor height, lighting, or exposure, capture both references again.

**The AS7341 and APDS-9960 share `0x39`.** The sketch selects only TCA channel 2
before initializing the AS7341. The APDS sensors can stay connected to their own
channels. Firmware that reads both sensor types must select only one `0x39`
channel at a time.

[Adafruit AS7341 Arduino guide](https://learn.adafruit.com/adafruit-as7341-10-channel-light-color-sensor-breakout/arduino)

## All three APDS-9960 sensors

Open `tests/APDS9960_All_Test/APDS9960_All_Test.ino` and use Serial Monitor at
**115200 baud**. The test holds motor standby low and reads one TCA channel at a
time. Left is **CH0**, right is **CH1**, and front is **CH7**. The AS7341 can stay
wired to CH2.

Check for three `READY` lines at startup. Bring a hand or wall sample toward
one sensor at a time and check that the matching raw count rises and then falls
as the target moves away. `valid` shows the PVALID bit. `sat=1` flags a saturated
sample and the sketch excludes it from the filter. An initialization error on
one channel leaves the other channels available for testing.

`side=OPEN` uses a filtered count of 9 or less, and `side=WALL` starts at 25.
`front=BLOCKED` starts at 40 on either the raw or filtered reading. On the side
sensors, `hand=HIGH` requires raw above 150, and `hand=RELEASED` requires raw
below 100. Use your maze walls at the intended distance to check the wall
thresholds before driving.

## APDS-9960 proximity test on channel 0

Connect the APDS-9960 SDA/SCL to the TCA9548A's **SD0/SC0** outputs, with
3.3 V compatible power and common ground. The TCA main bus stays on ESP32
GPIO21/GPIO22 as above. This standalone test keeps the original APDS gain and
LED boost settings, adds eight IR pulses, and uses smoothing factor `alpha = 0.65`.
Keep the motors off, upload the sketch, and move a hand toward and away from
the sensor. Serial Monitor at 115200 baud prints `CH0 raw` (0–255) and
`filtered` values every 100 ms. The raw reading should respond to the motion;
filtered readings respond more gradually. Earlier low channel-0 readings were
traced to the IR LED supply jumper; the jumper has now been soldered and the
APDS readings are reported working.

The original sketch combines two bytes starting at `PDATA` (`0x9C`), but
proximity data is one byte. The test reads only that byte so the adjacent
register cannot alter the reported value.

## Left wall tracking test

The [left-wall test](tests/Left_Wall_Follow_Test/README.md) uses APDS channel 0
and the documented motor pins. It waits for a successful APDS channel-0 read,
captures the current raw count as its target, then follows the straight wall
until BOOT, `x`, saturation, or an I2C error stops it. There is no run timer. It does not gate on PVALID or a fixed proximity count.
Press BOOT after startup or send `s` on USB Serial to start; BOOT or `x` stops it.
Idle output repeats the last stop reason. There is no front-wall or corner detection in this test.

## One tile movement test

The [one tile test](tests/Tile_Move_Test/README.md) is a separate sketch. It uses
70 mm wheels, a 300 mm tile length, the documented motor pins, and AS5600
encoders on TCA channels 5 (left) and 4 (right). As in the original startup,
show a hand to the left APDS (channel 0), then the right APDS (channel 1), and
remove the right hand to start the first forward tile. After that, send `g` for
one tile forward, `b` for one tile backward, or `x` to stop; BOOT starts or stops
a forward move. Each move tares both encoders before driving. Verify wheel
direction and encoder movement with the wheels lifted before floor testing.

The main firmware uses the robot's **left channel 0, right channel 1, and front
channel 7** APDS layout. Use the tile test separately when calibrating distance.

## Unified robot test and Wi-Fi dashboard

The [unified test](tests/Unified_Robot_Test/README.md) scans the TCA channels for
APDS-9960 sensors, reads the main-bus MPU6050/MPU6500 and both AS5600 encoders,
and provides short motor pulses on request. Motors remain in standby on startup.
Its local Wi-Fi access point serves a live status page at `http://192.168.4.1/`;
USB Serial also works. This is a standalone diagnostic sketch, not maze firmware.

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
- [APDS-9960 datasheet](https://cdn.sparkfun.com/assets/learn_tutorials/3/2/1/Avago-APDS-9960-datasheet.pdf)
- [TB6612FNG hookup guide](https://learn.sparkfun.com/tutorials/tb6612fng-hookup-guide/all?print=1)
