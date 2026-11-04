# Left wall tracking test

A standalone movement test for the 38-pin ESP32-WROOM-32 DevKit. It uses
the repo's motor pins and one APDS-9960 on TCA9548A channel 0. The CH0 sensor
must point at the **left wall**. There is no front sensor in this test, so it
does not turn at corners or avoid an obstacle ahead.

## Run it

1. Open Left_Wall_Follow_Test.ino in Arduino IDE. Select **ESP32 Dev Module**,
   upload, and open Serial Monitor at **115200 baud**.
2. With motor power off, check the IDLE lines. Place the robot at the distance
   you want from a straight left wall, with a clear path ahead.
3. Power the motor driver. Press the DevKit **BOOT** button **after startup**,
   or send **s** in Serial Monitor. The sketch captures the current APDS
   raw reading as its target and continues following until stopped.
4. Press BOOT again or send **x** to stop. The driver returns to
   standby when stopped.

Do not hold BOOT while powering or resetting the ESP32; GPIO0 is a boot strap
pin. The test requires a recent successful CH0 I2C read and no saturation.
It does not gate on the APDS PVALID bit or a fixed proximity count. Place the
robot beside the left wall before starting: it captures that reading as the
target, even if the count is around 20.

## Control used

The APDS proximity reading is filtered with alpha 0.65, as in the original
proximity.ino. The motor correction follows the left-wall branch of
moveStraightAligned() in motors.ino:

    error = filtered_left - target
    steering = KP * error + KD * (error - previous_error)
    left_pwm = base + steering
    right_pwm = base - steering

KP=0.85 and KD=1.5 come from config_lolins2.h. The target is captured when the
run starts because this assembled robot's sensor response is not yet
calibrated. For the first movement test, base PWM is reduced to 70 and steering
is limited to +/-25; edit BASE_PWM and MAX_STEERING near the top after observing
the wheels. The original left motor 0.985 trim is retained. The controller runs
every 20 ms. Motor output stops if an I2C read fails, saturation appears,
the user stops. There is no run timer. A single side sensor cannot confirm
that the wall is still present or detect an obstacle ahead.

## Pin map

| Connection | ESP32 |
| --- | --- |
| TCA SDA / SCL | GPIO21 / GPIO22 |
| APDS-9960 | TCA channel 0, I2C address 0x39 |
| Left AIN1 / AIN2 / PWMA | GPIO25 / GPIO26 / GPIO27 |
| Right BIN1 / BIN2 / PWMB | GPIO18 / GPIO19 / GPIO23 |
| TB6612 STBY | GPIO33 |
| Onboard BOOT button | GPIO0 |

This sketch uses USB Serial for telemetry and the onboard button for untethered
start/stop. Wi-Fi is omitted from the movement loop so web requests and radio
work do not change the timing of this first PD test. The unified hardware test
in this repo remains available for Wi-Fi sensor debugging.

Compiled for esp32:esp32:esp32 with ESP32 Arduino core 3.3.11: 23% flash,
7% RAM. Live wall-follow behavior still needs testing on the robot.

The APDS saturation flag is latched by the chip. The sketch clears it after
reporting a saturated sample, so a past event does not block all later starts.
The saturated sample is excluded from steering; the filter resets on the next
clean read. Idle output repeats last_stop so the reason remains visible. If saturation keeps
appearing, increase wall distance or reduce the APDS LED boost/gain.
