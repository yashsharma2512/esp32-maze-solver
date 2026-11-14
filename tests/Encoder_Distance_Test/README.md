# AS5600 distance move test

This is a standalone sketch for the 38-pin ESP32-WROOM-32. It drives both
motors a known distance using the AS5600 wheel encoders and prints each wheel's
counts and calculated travel. It does not start the maze firmware, APDS
sensors, or gyro.

## Run

1. Open `Encoder_Distance_Test.ino` in Arduino IDE and select **ESP32 Dev
   Module**. Keep motor power off while uploading. Open Serial Monitor at
   **115200 baud**.
2. Lift the wheels for the first check. The driver stays in standby until a
   command arrives. Confirm both encoders show `OK` magnet status in the idle
   output. Send `g` for a forward move or `b` for reverse. Send `x` to stop
   the current move. Send `stop` with **Newline** selected to disable motor
   drive until you reset the ESP32.
3. Once both wheels spin in the intended direction and their counts increase,
   place the robot on a clear straight path. Mark the center of an axle on the
   floor, send `g`, and measure the *physical* travel after it stops.

The target is `TEST_DISTANCE_MM = 300.0` near the top of the sketch. Change it
to the distance you want to test. The sketch tares both encoders before every
move and stops when the average of their calculated wheel travel reaches the
target. It also stops for an I2C error, unhealthy magnet status, a stalled
wheel, a large left/right mismatch, or a 15 second fault timeout. There is no
front obstacle sensor in this test, so leave a clear path ahead.

## Understand the numbers

The AS5600 reports **4096 counts per encoder revolution**. With the current
`WHEEL_DIAMETER_MM = 70.0`, one wheel revolution is about **219.9 mm**. A
300 mm move therefore asks for about **5588 counts** or **491 degrees** of
wheel rotation. `L` and `R` show each wheel's signed counts and absolute
calculated travel; `avg` is the mean of the two wheel travels. The sign of
the counts depends on encoder orientation. These calculations assume each
AS5600 magnet makes one revolution per wheel revolution.

The printed `avg` is calculated from the encoders, so `avg=300` is the stopping
condition, not an independent physical measurement. Compare it with a ruler.
If the target is 300 mm and the robot physically travels 270 mm, adjust the
effective diameter as follows:

    new WHEEL_DIAMETER_MM = old WHEEL_DIAMETER_MM * actual_mm / target_mm
    new WHEEL_DIAMETER_MM = 70 * 270 / 300 = 63 mm

Then repeat the floor test. If the robot overshoots, the effective diameter
increases by the same formula. Keep the measured result and use it to update
`WHEEL_DIAMETER_MM` in both this test and the main/tuning `header.h` files.

The existing `encoder_scaling = 57` in the ESP32 configs belongs to a
commented-out older encoder path. The active `MoveStrightToDistance()` uses
`WHEEL_DIAMETER_MM`, so changing `encoder_scaling` will not correct this move.

## Wiring

| Part | Connection |
| --- | --- |
| I2C SDA, SCL | ESP32 GPIO21, GPIO22 |
| TCA9548A | Main I2C bus, address 0x70 |
| Left AS5600 | TCA CH5, address 0x36 |
| Right AS5600 | TCA CH4, address 0x36 |
| Left TB6612 IN1, IN2, PWM | GPIO25, GPIO26, GPIO27 |
| Right TB6612 IN1, IN2, PWM | GPIO18, GPIO19, GPIO23 |
| TB6612 STBY | GPIO33 |

Use a common ground between the ESP32, sensors, driver, and motor power supply.
