# Unified ESP32 robot hardware test

Open Unified_Robot_Test.ino in Arduino IDE, select **ESP32 Dev Module**, choose
the board's USB port, and upload. Open Serial Monitor at **115200 baud**.
The sketch uses only libraries included with the ESP32 Arduino core:
Wire, WiFi, and WebServer.

## Pin map used

This is the pin map already documented in the repository:

| Function | ESP32 / mux connection |
| --- | --- |
| I2C SDA / SCL | GPIO21 / GPIO22 |
| TCA9548A | 0x70, A0/A1/A2 grounded |
| MPU6050 or MPU6500 | Main I2C bus, 0x68 or 0x69 |
| AS5600 left / right | TCA channel 5 / channel 4 |
| Motor driver left AIN1 / AIN2 / PWMA | GPIO25 / GPIO26 / GPIO27 |
| Motor driver right BIN1 / BIN2 / PWMB | GPIO18 / GPIO19 / GPIO23 |
| Motor driver STBY | GPIO33 |
| APDS-9960 | Discovered automatically on any TCA channel |

The repo only assigns an APDS to channel 0 so far. Other detected APDS modules
are shown by mux channel, not by front/diagonal physical position. No unverified
buzzer, click switch, NeoPixel, or UART GPIO is used.

## What it does

At startup the sketch holds STBY LOW, discovers TCA/IMU/APDS/AS5600 devices,
initializes APDS proximity conversion, and tares the two encoders. It reports:

- IMU identity, acceleration in g, and rotation rate in degrees per second.
- Each APDS channel's raw 0–255 proximity, filtered reading, PVALID, and saturation.
- Encoder angle, accumulated turns, magnetic magnitude, AGC, and magnet status.

Sensor values update internally every 100 ms and print over USB every second.
The letter z in Serial Monitor tares both encoders again.

## Wi-Fi debugging

The ESP32 starts a local Wi-Fi access point:

- Network: **MazeRobot-Test**
- Password: **maze-test-123**
- Web page: **http://192.168.4.1/**

Connect a phone or laptop to that network, then open the page. The status
updates once a second. The Wi-Fi page and USB Serial show the same latest sensor
report. No router or internet connection is needed. Change the password in
the sketch if you want a different one.

## Motor tests

**Lift both wheels before pressing a motor button.** Motors never run
automatically. Each web button or USB command drives at PWM 120 for 350 ms
and then lowers STBY. USB commands:

- 1 / 2: left forward / reverse
- 3 / 4: right forward / reverse
- 5 / 6: both forward / reverse
- z: tare both encoders

The short motor pulse waits without making I2C or Wi-Fi calls, so a slow sensor
read cannot extend it. Motor power must still come from the motor driver supply
with common ground.

## Wi-Fi impact

This is a **standalone diagnostic sketch**. Uploading it replaces the current
firmware until another sketch is uploaded. It does not write settings into the
maze firmware or change the wiring. Wi-Fi uses no extra GPIO and this test
does not use ADC2, so there is no pin or ADC conflict with the documented
motor/I2C map. Wi-Fi does use extra RAM, CPU time, and supply current. Do not
use its sample timing to tune the real-time maze control loop. Compile check:
ESP32 Arduino core 3.3.11, esp32:esp32:esp32; 73% flash and 14% RAM.

The earlier APDS channel-0 test showed valid conversions but raw values around
0–5 near a target; the encoder test previously reported weak magnets. Confirm
these are fixed before using APDS wall thresholds or wheel distance control.

The APDS saturation flag is latched by the chip. This sketch clears it after
reporting a saturated sample, allowing later samples to show whether the
condition persists.
