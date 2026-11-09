#pragma once
#include "Settings.h"
#include <Wire.h>
#include <Adafruit_AS7341.h>
#include <MPU6050_light.h>

// Shared readings and run state. The named Arduino tabs hold the definitions.

struct Proximity {
  const char *name;
  uint8_t channel;
  uint8_t raw = 0;
  float filtered = 0;
  bool filterReady = false;
};
extern Proximity leftAPDS, rightAPDS, frontAPDS;  // WallSensors.ino

struct Encoder {
  uint8_t channel;
  int16_t previous = -1;
  int32_t counts = 0;
  uint8_t status = 0;
  bool readOK = false;
};
extern Encoder leftEncoder, rightEncoder;  // Encoders.ino

extern MPU6050 mpu;                 // Gyro.ino
extern int leftPWM, rightPWM;       // Motors.ino
extern bool busReady;               // 02_Startup.ino
extern uint32_t lastSample;         // 03_Sensors.ino
extern bool running;                // 04_Controls.ino
extern const char *phase, *lastStop; // 04_Controls.ino
extern bool hadLeftWall;            // 06_WallFollow.ino
extern int previousWallError;       // 06_WallFollow.ino
extern bool colorCalibrationActive; // ColorCalibration.ino

// Turns and steering only need wall readings and heading.
bool readSensors(bool needEncoders = false);
bool updateDuringMove(bool needEncoders = false);
