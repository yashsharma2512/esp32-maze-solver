#pragma once
#include <Arduino.h>

// STEP 1 - Wiring. Motor pins are IN1, IN2, and PWM.
constexpr int SDA_PIN = 21, SCL_PIN = 22;
constexpr int LEFT_FORWARD = 25, LEFT_REVERSE = 26, LEFT_SPEED = 27;
constexpr int RIGHT_FORWARD = 18, RIGHT_REVERSE = 19, RIGHT_SPEED = 23;
constexpr int MOTOR_STANDBY = 33;

// I2C addresses are separate from the TCA's numbered channels.
constexpr uint8_t TCA_ADDR = 0x70, APDS_ADDR = 0x39, AS5600_ADDR = 0x36;
constexpr uint8_t COLOR_CH = 2;
constexpr float APDS_ALPHA = 0.65f;

// Navigation and timing.
// Wall-follow settings from the original robot.
constexpr int BASE_PWM = 100;
constexpr int FRONT_WALL = 40, SIDE_OPEN = 9, SIDE_WALL = 25;
constexpr int SIDE_TARGET = 50;
constexpr float WALL_KP = 0.85f, WALL_KD = 1.5f;
constexpr float GYRO_ASSIST = 1.5f;
constexpr int GYRO_Z_SIGN = 1;  // Set to -1 if the IMU's Z axis points down.

// Corner and turn settings from the original robot.
constexpr float WHEEL_DIAMETER_MM = 70.0f;
constexpr float CORNER_ADVANCE_MM = 220.0f, CORNER_BACKUP_MM = 30.0f;
constexpr bool TURN_LEFT = true, TURN_RIGHT = false;
constexpr float TURN_KP = 12.5f, TURN_KD = 0.2f;
constexpr uint32_t TURN_TIMEOUT_MS = 1100, TURN_HOLD_MS = 200;
constexpr uint32_t DISTANCE_TIMEOUT_MS = 4500;

constexpr int BOOT_BUTTON = 0;
constexpr uint32_t SAMPLE_MS = 10, SENSOR_LIMIT_MS = 120;
constexpr int HAND_NEAR = 150, HAND_AWAY = 100;
