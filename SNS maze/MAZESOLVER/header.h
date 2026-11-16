/*
 * ENABLE PWM ON PIN0 ARDUINO ZERO
 * FROM
 * { PORTA, 11, PIO_SERCOM, (PIN_ATTR_DIGITAL), No_ADC_Channel, NOT_ON_PWM, NOT_ON_TIMER, EXTERNAL_INT_11 }, // RX: SERCOM0/PAD[3]
 * TO
 * {PORTA, 11, PIO_DIGITAL, (PIN_ATTR_DIGITAL | PIN_ATTR_PWM | PIN_ATTR_TIMER), ADC_Channel19, PWM1_CH1, TCC1_CH1, EXTERNAL_INT_NONE},
 *
 * add the following to MPU6050_light.h
 * void resetAngleZ(){angleZ=0;};
 */

#define DEBUG

// #include <Servo.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <Adafruit_NeoPixel.h>
#include "maze.h"
#include "as5600.h"

#define LED_PIN 4
#define LED_COUNT 2
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Servo dropper;
#define dropperAngle 85

/////////////////////////////////////////////////////////////////////////////////
// select config file for the board in use //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// #include "config/config_pico.h"
// #include "config/config_rdplite_v3.h"
#include "config/config_lolins2.h"
#include "src/NiclaVision/NiclaVision.h"
#include "src/NiclaVision/TargetPixels.h"
#include "src/NiclaVision/RescueSequence.h"
/////////////////////////////////////////////////////////////////////////////////

unsigned long _tick = 0;

bool state = false;

#define SerialDebug Serial

MPU6050 mpu(Wire);

// APDS9960 I2C address
#define APDS9960_ADDRESS 0x39

// APDS9960 register addresses
#define APDS9960_REG_ENABLE 0x80
#define APDS9960_REG_PDATA 0x9C
#define APDS9960_REG_CONFIG2 0x90
#define APDS9960_REG_CONFIG3 0x9F
#define APDS9960_REG_CONTROL 0x8F

#define APDS9960_REG_COUNTR 0x8E

constexpr size_t COLOR_BAND_COUNT = 4;  // APDS9960 clear, red, green, and blue.
enum class ColorSampleStatus { Waiting, Ready, Failed };

volatile int encoderPos = 0;
volatile int lastEncoded = 0;

#define LEFT -1
#define RIGHT 1
#define UTURN 2

#define FRONTWALL 1
#define ADJECENTWALL 2

String heading[] = { "N", "E", "S", "W" };

int HEADING = 0;

int GLOBAL_X = 0;
int GLOBAL_Y = 0;

int p_move = 0;
int d_move = 0;
int pe_move = 0;

bool prevWallLeft = false;
bool prevWallRight = false;

int fl = 0;
int fr = 0;
int dl = 0;
int dr = 0;

float fl_filtered = 0;
float fr_filtered = 0;
float dl_filtered = 0;
float dr_filtered = 0;

int prevTile = 0;

const float alpha_ir = 0.9;
float calibrated_ir = 0.0;

float offset_ir=0;

MagneticEncoder encLeft(5, true);
MagneticEncoder encRight(6, false);

#define WHEEL_DIAMETER_MM 70.0  // Example wheel diameter in mm
#define KP 1.5                  // Proportional gain constant (tune as needed)

void alterLEDstate() {
  state = !state;
  if (LED13 >= 0) digitalWrite(LED13, state);
}

void _beep() {
  if (BUZZPIN >= 0) digitalWrite(BUZZPIN, HIGH);
  delay(40);
  if (BUZZPIN >= 0) digitalWrite(BUZZPIN, LOW);
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
