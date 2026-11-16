#include "header.h"

bool IR_CALIBRATE = false;
bool AI_ENABLE = true;

// Standby
#define STBY 33

void setup() {
  /* Initialize Serial module */
  SerialDebug.begin(115200);

  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  // SerialUSB.begin(115200);

  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);

  /* Set motor outputs */
  setMotorIOs();
  /* Initialize I2C module */
  beginWire();

  /* Initialize MPU6050 module */
  beginMpu();

  /* Initialize neopixel LEDs*/
  initRGB();

  /* Set pin 7, 13 as output */
  pinMode(BUZZPIN, OUTPUT);
  pinMode(LED13, OUTPUT);

  pinMode(LEFTLED, OUTPUT);
  pinMode(RIGHTLED, OUTPUT);

  pinMode(enc1, INPUT_PULLUP);
  pinMode(enc2, INPUT_PULLUP);

  pinMode(LEFTCLICK, INPUT_PULLUP);
  pinMode(RIGHTCLICK, INPUT_PULLUP);

  // dropper.attach(2);
  // delay(500);
  // dropper.write(85);

  moveMotor(0, 0);

  // reset both encoders to zero degrees
  encLeft.init();
  encRight.init();

  analogReadResolution(16);

  /* Scan I2C devices (uncomment to check connected I2C device addresses) */
  // testI2C();


  /* Initializes all proximity sensors */
  initAPDS(FL);
  initAPDS(DL);
  initAPDS(FR);
  initAPDS(DR);
  setupColorSensor();

  while (0) {
    updateProximitySensors();
    Serial.print(dl_filtered);
    Serial.print(",");
    Serial.print(fl_filtered);
    Serial.print(",");
    Serial.println(dr_filtered);

    delay(10);
  }

  showColor(strip.Color(0, 0, 100));
  /* let the bot rest and wait for user input from proximity sensor > diagonal left */
  waitForUserInput1();

  showColor(strip.Color(0, 100, 100));

  Serial.println("START1");

  // /* calibrate offset for IR sensor */
  // bool state = false;
  // for (int i = 0; i < 100; i++) {
  //   int x = analogRead(A4);
  //   int y = analogRead(A5);

  //   long z = (x + y) / 2;
  //   calibrated_ir = alpha_ir * (calibrated_ir) + (1 - alpha_ir) * z;
  //   state = !state;
  //   digitalWrite(LED13, state);
  //   delay(10);
  // }
  // offset_ir = calibrated_ir;

  /* Calculate gyroscope offsets and wait for user input from proximity sensor > diagonal right */
  mpu.calcOffsets(true, true);  // gyro and accel
  mpu.resetAngleZ();

  encLeft.tare();
  encRight.tare();


  // while (IR_CALIBRATE) {
  //   interpretIR(true);
  //   delay(5);
  // }

  Serial.println("MPU calibrated.");

  waitForUserInput2();
  startFloorDetection();

  showColor(strip.Color(0, 50, 0));

  Serial.println("START2");

  encoderPos = 0;
  enableISR();

  SerialDebug.println("Start");
  digitalWrite(STBY, HIGH);

  while (0) {
    mpu.update();
    updateProximitySensors();
    moveStraightAligned();
    
    victimDetection();
  }

  digitalWrite(STBY, HIGH);
  /* test the rotation accuracy using diagonal sensors. comment to disable testing. */
  // testRotation();

  /* Run main algorithm
  Note: function is blocking*/
  // solveMaze();
  while (1) {
    // MoveStrightToDistance(500);
    wallFollow(LEFT);
    // delayWithSensorUpdate(500);
  }
  /* Stop the motors*/
  moveMotor(0, 0);

  /* indicate finish state by blinking both leds continously. */
  while (1) {
    state = !state;
    digitalWrite(LEFTLED, state);
    digitalWrite(RIGHTLED, state);
    delay(400);
  }
}

void loop() {
  /* Nothing to do here */
  delay(1);
}
