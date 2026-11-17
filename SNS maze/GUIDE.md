# SNS maze: wiring, firmware, and first run

This guide describes the **LOLIN S2 Mini + Nicla Vision** build in this folder. The
pin assignments below come from the selected
[`config_lolins2.h`](MAZESOLVER/config/config_lolins2.h), plus the I²C and LED
definitions in `proximity.ino` and `header.h`. Check the physical harness against
these assignments before powering the motors.

## Which code goes on which board

| Board | File to load | Tool | What it does |
| --- | --- | --- | --- |
| LOLIN S2 Mini | [`MAZESOLVER/MAZESOLVER.ino`](MAZESOLVER/MAZESOLVER.ino) | Arduino IDE, board **LOLIN S2 Mini** | Reads sensors, drives motors, receives camera packets, handles floor tiles and rescue kits. |
| Nicla Vision | [`NiclaVision/main.py`](NiclaVision/main.py) saved **on the Nicla as `main.py`** | OpenMV IDE | Captures images, classifies rings and Greek letters, sends UART4 packets over ESLOV SDA. |

Keep the entire `MAZESOLVER` directory together: Arduino builds all its `.ino`
tabs and the `src/NiclaVision` headers as one sketch. `header.h` currently selects
`config/config_lolins2.h`. The other config headers are for other boards and must
not be mixed with this wiring. The separate Wi-Fi robot's
`vision/NiclaVision/robot_uart.py` uses a different UART/pin arrangement; **do not
put it on the SNS Nicla**. Check the actual file stored on the Nicla if the
camera worked previously but the LOLIN now receives zero bytes.

## Wiring overview

Connect **all grounds**: LOLIN, TCA9548A, motor driver, Nicla, NeoPixels and
the separate servo supply. Use supplies and logic levels supported by each
module. The table names signal pins; power pins depend on the particular
sensor, driver, LED and servo modules in your robot. Switch power off before
rewiring. Do not power the servo from a LOLIN GPIO.

### Nicla camera link: one-way UART at 115200 baud

| Nicla Vision ESLOV contact | LOLIN S2 Mini | Purpose |
| --- | --- | --- |
| **J5-4 SDA_EXT** (UART4 TX) | **GPIO12**, `NICLA_RX_PIN` (UART1 RX) | Camera CSV packets from Nicla to LOLIN. |
| **J5-5 GND** | **GND** | Shared signal reference. |

Power the Nicla through an appropriate supported supply or its USB during a
bench test. ESLOV SCL_EXT and INT are unused for this one-way connection.
LOLIN GPIO14 is configured as UART1 TX but the SNS script does not read a
return channel, so no return wire is needed. **Do not connect Nicla SDA_EXT
to GPIO16**: that pin is the separate sensor I²C bus. The Nicla's UART9/D1
program used by another robot is not the SNS ESLOV UART4 program.

The Nicla script starts capturing and transmitting when it runs. The LOLIN
starts **receiving and acting on packets only after `START2`**. There is no
LOLIN-to-Nicla arm command in this one-way wiring.

### I²C sensors and multiplexer

| LOLIN signal | Connect to | Notes |
| --- | --- | --- |
| **GPIO16** | TCA9548A upstream SDA; MPU6050 SDA | Main I²C SDA (`SDA_PIN` in `proximity.ino`). |
| **GPIO17** | TCA9548A upstream SCL; MPU6050 SCL | Main I²C SCL (`SCL_PIN`); sketch runs it at 400 kHz. |
| **GND** | TCA9548A and MPU6050 GND | Common ground. |

The TCA9548A address in the code is **0x70**. On each downstream channel,
connect the device's SDA to **SDn**, SCL to **SCn**, plus suitable power and
ground. The sketch uses these channels:

| TCA channel | Device and role | Defined in |
| --- | --- | --- |
| **CH2** | Diagonal left APDS9960 (`DL`) | `config_lolins2.h` |
| **CH3** | Front APDS9960 reading used for both `FL` and `FR` in the current config | `config_lolins2.h` |
| **CH4** | Diagonal right APDS9960 (`DR`) | `config_lolins2.h` |
| **CH5** | Left AS5600 magnetic wheel encoder | `header.h` |
| **CH6** | Right AS5600 magnetic wheel encoder | `header.h` |
| **CH7** | Downward floor-color APDS9960 (`COLOR_CH`) | `config_lolins2.h` |

The current SNS config sets **both `FL` and `FR` to CH3**. If the robot has
**one** physical front APDS9960, connect it to CH3; both software readings
will intentionally be identical. If it has **two** physical front APDS9960
modules, do not wire both to CH3: they share I²C address 0x39 and would
collide. Put one on a free TCA channel and update its `FL` or `FR` definition
in `config_lolins2.h` to match the actual harness. Keep CH5/CH6 for the
AS5600 encoders and CH7 for the floor sensor.

The floor sensor's SDA/SCL go to **SD7/SC7**, not directly to the upstream
bus. The AS5600 encoders are read over CH5/CH6 at I²C address 0x36. GPIO6
and GPIO7 are declared as `enc1`/`enc2` input pins, but their interrupt code
is currently disabled; they are not substitutes for the two AS5600 links.

### Motors, LEDs and dropper

| LOLIN GPIO | Connect to | Code name |
| --- | --- | --- |
| **35, 36, 37** | Left motor driver forward, reverse, PWM inputs | `RMF`, `RMR`, `PWMR` |
| **18, 34, 21** | Right motor driver forward, reverse, PWM inputs | `LMF`, `LMR`, `PWML` |
| **33** | Motor driver standby/enable input | `STBY`; low until armed |
| **4** | Data input of the two chained NeoPixels | `LED_PIN`, `LED_COUNT=2` |
| **2** | Rescue servo signal | `DROPPER_PIN` |
| **39, 40** | Left and right clicker inputs, if fitted | `LEFTCLICK`, `RIGHTCLICK` |

The left/right motor **code names** are historical: follow the physical-side
comments in `config_lolins2.h` and confirm wheel direction with the wheels
raised. Connect the driver outputs to the corresponding motors and supply
the driver from the motor supply appropriate to your hardware. The servo
needs a suitable separate supply and shared ground. Its current positions
are **0° home, 90° release**, set in
[`DropperConfig.h`](MAZESOLVER/src/NiclaVision/DropperConfig.h). Verify the
mechanism with an empty hopper before loading kits. `BUZZPIN`, `LED13`,
`LEFTLED` and `RIGHTLED` are set to `-1`, so no GPIO is assigned to them.

## Install and run the firmware

1. **Wire and inspect.** Check power polarity, common grounds, the TCA
   channel assignments, the ESLOV SDA_EXT-to-GPIO12 wire, and motor direction.
   Raise the wheels and leave the kit hopper empty for the first run.
2. **LOLIN:** Open `MAZESOLVER/MAZESOLVER.ino` in Arduino IDE. Select
   **LOLIN S2 Mini** and its USB port. Install **Adafruit NeoPixel** and
   **MPU6050_light**. Upload the complete sketch. It uses ESP32 Arduino core
   LEDC functions for motor and servo PWM; use a core version that provides
   `ledcAttachChannel`. The original sketch also calls
   `mpu.resetAngleZ()`; if your `MPU6050_light` version lacks it, see the
   existing note in [`MAZESOLVER/README.md`](MAZESOLVER/README.md).
3. **Nicla:** Open the SNS `NiclaVision/main.py` in OpenMV IDE, connect the
   Nicla, and save that file **to the device** as `main.py`. Then reset/run
   it. Confirm `Nicla SNS UART4 TX ready on ESLOV SDA`,
   `Nicla camera: CSI ready`, and an increasing `Nicla SNS UART4 TX bytes`
   count in the OpenMV Serial Terminal. A preview alone does not prove that
   the correct SNS script is installed.
4. **LOLIN startup:** Open its USB Serial Monitor at **115200 baud**, with
   **New Line** selected. Before the first hand gesture, optionally run floor
   calibration below. Show a hand to the **diagonal left** APDS: `START1`
   appears. Keep the robot still while the MPU calibrates. Then show a hand
   to the **diagonal right** APDS and withdraw it: `START2` appears, motor
   standby goes high, UART1 opens on GPIO12, and maze driving begins.
5. **Camera check:** After `START2`, send `camera` followed by a newline on
   the LOLIN Serial Monitor. Expect `Camera UART: ... bytes received, ...
   packets accepted since boot.` Repeating the command should show both
   counts increasing. The Nicla sends **clear packets even when it sees no
   target**; no `COGNITIVE`/`PSO` detection line in an empty scene is normal.

The camera command is available **after arming**, while the maze code is
servicing the camera. This SNS sketch is a USB Serial workflow; it does not
provide the Wi-Fi sketch's web dashboard or `start`/`stop` commands.

### Optional floor-color calibration

At startup, before the diagonal-left gesture, send `c` with a newline. This
keeps the motors stopped. Under steady lighting and at the mounted sensor
height, place each surface under the downward APDS9960 and send:

```text
save floor
save black
save blue
list
read
exit
```

`read` prints raw clear/red/green/blue bands, the closest saved reference and
the `Driving match`. Check it on each surface. `help` lists commands;
`delete black` removes one reference. Samples are stored in the LOLIN's
emulated EEPROM. `exit` returns to the first hand gesture. Floor actions
require a saved `floor` (or `white`) reference and at least one tile
reference. A black tile triggers a U-turn; blue pauses for five seconds.

## Target and dropper behavior

The Nicla sends two newline-delimited `type,stable_ms,x,y` CSV packets per
frame, one for cognitive rings and one for Greek letters. The LOLIN requires
**three matching recent packets** before confirming a target. It logs
`COGNITIVE` or `PSO`, then `RESCUE` when it handles an encounter.

| Target | Code type | Kits requested |
| --- | ---: | ---: |
| Stable ring or Ψ (Psi) | 0 or 4 | 1 |
| Unharmed ring or Ω (Omega) | 1 or 5 | 0 |
| Harmed ring or Φ (Phi) | 2 or 3 | 2 |

For a rescue, the LOLIN stops the motors, moves the servo to release and
back once per requested kit, then waits for a **confirmed clear scene** before
handling another target. A log of requested kits does not prove that a
physical kit dropped; verify the mechanism and tune the servo angles. See
[`TESTING.md`](TESTING.md) for a staged bench check and
[`NiclaVision/README.md`](NiclaVision/README.md) for the detector notes.

## If something does not respond

| Observation | Check |
| --- | --- |
| No `START1` or `START2` | Diagonal APDS sensors on CH2/CH4, TCA power and bus, and hand distance. The thresholds in `proximity.ino` are 150 for each gesture. |
| Nicla preview works but LOLIN camera count stays at **0 bytes** after `START2` | Verify the **on-device** `main.py` is the SNS UART4 script, its TX count rises, J5-4 SDA_EXT reaches GPIO12, and grounds are common. A UART9/D1 script can show a preview yet send nothing to this wire. |
| LOLIN bytes rise but accepted packets stay at 0 | Check both sides use 115200 baud and the SNS CSV packet format; inspect the Nicla Serial Terminal for write errors. |
| Bytes and accepted packets rise but no target is reported | The link is working. Present a clear target under course lighting; inspect the OpenMV preview, focus, target size and detector output. Three matching recent packets are required. |
| Floor sensor unavailable or `read` fails | Check CH7 SD7/SC7, power and ground, and lighting. |
| Rescue log appears but no kit falls | Check servo signal on GPIO2, separate servo power/common ground, hopper loading, and home/release angles. |

For development-only checks, run `python3 NiclaVision/tests/test_simulated_targets.py`
from this folder with Pillow installed. It checks synthetic glyphs, rings,
UART selection and packet formatting; it does not test the physical camera,
wiring, motors or dropper.
